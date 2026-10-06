#pragma once

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace rde
{

inline double besselI0 (double x) noexcept
{
    double sum = 1.0, term = 1.0;
    const double q = x * x * 0.25;

    for (int k = 1; k < 64; ++k)
    {
        term *= q / (double (k) * double (k));
        sum += term;

        if (term < sum * 1.0e-12)
            break;
    }

    return sum;
}

/** Target magnitude (linear) of the detection band: flat between lo and hi,
    falling at slopeDbPerOct outside, floored at -100 dB. */
inline double bandMagnitude (double f, double lo, double hi, double slopeDbPerOct) noexcept
{
    constexpr double floorDb = -100.0;
    double db = 0.0;

    if (f < lo)
        db = f <= 0.0 ? floorDb : -slopeDbPerOct * std::log2 (lo / f);
    else if (f > hi)
        db = -slopeDbPerOct * std::log2 (f / hi);

    return std::pow (10.0, std::max (db, floorDb) / 20.0);
}

/** Computes band edges from centre/width, clamped to the usable spectrum. */
inline void bandEdges (double centreHz, double widthOct, double sampleRate, double& lo, double& hi) noexcept
{
    lo = centreHz * std::exp2 (-0.5 * widthOct);
    hi = centreHz * std::exp2 (0.5 * widthOct);
    hi = std::min (hi, 0.4995 * sampleRate);
    lo = std::clamp (lo, 10.0, hi * 0.95);
}

/** Designs symmetric (linear-phase) band-pass FIRs by frequency sampling + Kaiser window. */
class LinearPhaseFirDesigner
{
public:
    void prepare (int numTaps)
    {
        jassert (numTaps % 2 == 1);
        taps = numTaps;
        half = (numTaps - 1) / 2;

        int order = 12;
        while ((1 << order) < 4 * numTaps)
            ++order;

        fft = std::make_unique<juce::dsp::FFT> (order);
        fftSize = 1 << order;
        work.assign ((size_t) (2 * fftSize), 0.0f);

        window.resize ((size_t) taps);
        constexpr double beta = 6.0;
        const double denom = besselI0 (beta);

        for (int n = 0; n < taps; ++n)
        {
            const double r = double (n - half) / double (half);
            window[(size_t) n] = float (besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / denom);
        }

        // Calibrate the inverse transform's scaling: a flat spectrum must give a unit impulse.
        std::fill (work.begin(), work.end(), 0.0f);
        for (int k = 0; k <= fftSize / 2; ++k)
            work[(size_t) (2 * k)] = 1.0f;

        fft->performRealOnlyInverseTransform (work.data());
        inverseScale = work[0] != 0.0f ? 1.0f / work[0] : 1.0f;
    }

    int getNumTaps() const noexcept { return taps; }

    /** Writes getNumTaps() coefficients into h. Allocation free. */
    void design (float* h, double sampleRate, double lo, double hi, double slopeDbPerOct) noexcept
    {
        std::fill (work.begin(), work.end(), 0.0f);

        for (int k = 0; k <= fftSize / 2; ++k)
        {
            const double f = sampleRate * double (k) / double (fftSize);
            work[(size_t) (2 * k)] = float (bandMagnitude (f, lo, hi, slopeDbPerOct));
        }

        fft->performRealOnlyInverseTransform (work.data());

        // Zero-phase impulse is centred on index 0 (circular): shift it to the middle tap.
        for (int n = 0; n < taps; ++n)
        {
            const int idx = (n - half + fftSize) % fftSize;
            h[n] = work[(size_t) idx] * inverseScale * window[(size_t) n];
        }
    }

private:
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> work, window;
    int taps = 0, half = 0, fftSize = 0;
    float inverseScale = 1.0f;
};

/** Splits a signal into the linear-phase "inside" band and the latency-matched dry signal.
    "Outside" is always delayed - inside, so inside + outside reconstructs the input exactly.
    Coefficient changes are crossfaded to avoid zipper noise. */
class BandSplitter
{
public:
    void prepare (double sampleRate, int halfLength, int numChannels)
    {
        fs = sampleRate;
        half = std::max (1, halfLength);
        taps = 2 * half + 1;
        designer.prepare (taps);

        for (auto& c : coeffs)
            c.assign ((size_t) taps, 0.0f);

        history.assign ((size_t) std::max (1, numChannels), std::vector<float> ((size_t) (2 * taps), 0.0f));
        writePos.assign (history.size(), 0);
        fadeLength = std::max (64, int (0.005 * fs));
        hasDesign = fading = pending = false;
        active = 0;
        fadePos = 0;
    }

    void reset()
    {
        for (auto& h : history)
            std::fill (h.begin(), h.end(), 0.0f);

        std::fill (writePos.begin(), writePos.end(), 0);
    }

    int getLatency() const noexcept { return half; }
    int getNumTaps() const noexcept { return taps; }
    const std::vector<float>& getActiveCoefficients() const noexcept { return coeffs[(size_t) active]; }

    void setBand (double lo, double hi, double slopeDbPerOct) noexcept
    {
        targetLo = lo;
        targetHi = hi;
        targetSlope = slopeDbPerOct;
        pending = true;
    }

    void beginBlock() noexcept
    {
        if (! hasDesign)
        {
            designer.design (coeffs[(size_t) active].data(), fs, targetLo, targetHi, targetSlope);
            hasDesign = true;
            pending = false;
        }
        else if (pending && ! fading)
        {
            designer.design (coeffs[(size_t) (1 - active)].data(), fs, targetLo, targetHi, targetSlope);
            fading = true;
            fadePos = 0;
            pending = false;
        }
    }

    void processChannel (int ch, const float* in, float* inside, float* delayed, int n) noexcept
    {
        auto& hist = history[(size_t) ch];
        int wp = writePos[(size_t) ch];
        const float* c0 = coeffs[(size_t) active].data();
        const float* c1 = coeffs[(size_t) (1 - active)].data();

        for (int i = 0; i < n; ++i)
        {
            hist[(size_t) wp] = in[i];
            hist[(size_t) (wp + taps)] = in[i];

            const float* p = hist.data() + wp + 1; // oldest .. newest, contiguous
            delayed[i] = p[half];

            float y = fir (c0, p);

            if (fading)
            {
                const float a = std::min (1.0f, float (fadePos + i + 1) / float (fadeLength));
                y += a * (fir (c1, p) - y);
            }

            inside[i] = y;

            if (++wp == taps)
                wp = 0;
        }

        writePos[(size_t) ch] = wp;
    }

    void endBlock (int n) noexcept
    {
        if (! fading)
            return;

        fadePos += n;

        if (fadePos >= fadeLength)
        {
            active = 1 - active;
            fading = false;
        }
    }

private:
    float fir (const float* c, const float* p) const noexcept
    {
        float acc = c[half] * p[half];

        for (int k = 0; k < half; ++k)
            acc += c[k] * (p[k] + p[taps - 1 - k]);

        return acc;
    }

    LinearPhaseFirDesigner designer;
    std::vector<float> coeffs[2];
    std::vector<std::vector<float>> history;
    std::vector<int> writePos;
    double fs = 48000.0, targetLo = 4000.0, targetHi = 10000.0, targetSlope = 48.0;
    int half = 1, taps = 3, active = 0, fadeLength = 256, fadePos = 0;
    bool hasDesign = false, fading = false, pending = false;
};

} // namespace rde
