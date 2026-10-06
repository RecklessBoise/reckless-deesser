// Unit tests for the Reckless DeEsser DSP engine. Plain runner, exit code = number of failures.

#include "dsp/DeEsserEngine.h"

#include <cstdio>
#include <functional>
#include <random>
#include <string>

namespace
{
int failures = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (! (cond)) { ++failures; std::printf ("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
    } while (false)

#define CHECK_NEAR(a, b, tol)                                                                          \
    do {                                                                                               \
        const double va_ = (a), vb_ = (b);                                                             \
        if (std::abs (va_ - vb_) > (tol)) {                                                            \
            ++failures;                                                                                \
            std::printf ("  FAIL %s:%d  %s = %g, expected %g +/- %g\n", __FILE__, __LINE__, #a, va_, vb_, (double) (tol)); \
        }                                                                                              \
    } while (false)

void run (const char* name, const std::function<void()>& fn)
{
    const int before = failures;
    fn();
    std::printf ("%s %s\n", failures == before ? "[ OK ]" : "[FAIL]", name);
}

double rmsDb (const juce::AudioBuffer<float>& b, int ch, int start, int n)
{
    double acc = 0.0;
    for (int i = start; i < start + n; ++i)
        acc += double (b.getSample (ch, i)) * b.getSample (ch, i);
    return 10.0 * std::log10 (acc / n + 1.0e-20);
}

double firResponseDb (const std::vector<float>& h, double f, double fs)
{
    const int half = (int) (h.size() - 1) / 2;
    const double w = juce::MathConstants<double>::twoPi * f / fs;
    double r = h[(size_t) half];
    for (int k = 1; k <= half; ++k)
        r += 2.0 * h[(size_t) (half + k)] * std::cos (w * k);
    return 20.0 * std::log10 (std::abs (r) + 1.0e-12);
}

juce::AudioBuffer<float> sine (double fs, double freq, float amp, int n, int channels = 2)
{
    juce::AudioBuffer<float> b (channels, n);
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < n; ++i)
            b.setSample (ch, i, amp * (float) std::sin (juce::MathConstants<double>::twoPi * freq * i / fs));
    return b;
}

void processInBlocks (rde::DeEsserEngine& e, juce::AudioBuffer<float>& b, const rde::EngineParams& p, int block = 256)
{
    for (int pos = 0; pos < b.getNumSamples(); pos += block)
    {
        const int n = std::min (block, b.getNumSamples() - pos);
        juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), b.getNumChannels(), pos, n);
        e.process (view, p);
    }
}

rde::EngineParams sibilanceParams()
{
    rde::EngineParams p;
    p.freqHz = 6000.0f;
    p.widthOct = 1.0f;
    p.slopeDbPerOct = 48.0f;
    p.thresholdDb = -30.0f;
    p.ratio = std::numeric_limits<float>::infinity();
    p.kneeDb = 0.0f;
    p.attackMs = 0.5f;
    p.holdS = 0.0f;
    p.releaseS = 0.05f;
    p.autoTrack = false;
    return p;
}
} // namespace

int main()
{
    run ("gain curve: ratio, cut and soft knee", [] {
        CHECK_NEAR (rde::gainReductionDb (-10.0f, 4.0f, 0.0f), 0.0, 1e-6);
        CHECK_NEAR (rde::gainReductionDb (12.0f, 4.0f, 0.0f), -9.0, 1e-5);
        CHECK_NEAR (rde::gainReductionDb (6.0f, std::numeric_limits<float>::infinity(), 0.0f), -6.0, 1e-5);
        // knee is continuous at both edges
        CHECK_NEAR (rde::gainReductionDb (2.5f, 4.0f, 5.0f), rde::gainReductionDb (2.5001f, 4.0f, 5.0f), 1e-3);
        CHECK_NEAR (rde::gainReductionDb (-2.5f, 4.0f, 5.0f), 0.0, 1e-5);
        CHECK (rde::gainReductionDb (0.0f, 4.0f, 6.0f) < 0.0f);
    });

    run ("latency stays below 2 ms at every common rate", [] {
        for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        {
            rde::DeEsserEngine e;
            e.prepare (fs, 512, 2);
            CHECK (e.getLatencySamples() / fs < 0.002);
            CHECK (e.getLatencySamples() > 0);
        }
    });

    run ("linear-phase band filter: symmetric, flat in band, steep outside", [] {
        const double fs = 48000.0;
        rde::BandSplitter s;
        s.prepare (fs, rde::DeEsserEngine::halfLengthFor (fs), 1);
        s.setBand (4000.0, 8000.0, 48.0);
        s.beginBlock();
        const auto& h = s.getActiveCoefficients();

        for (size_t k = 0; k < h.size() / 2; ++k)
            CHECK_NEAR (h[k], h[h.size() - 1 - k], 1e-7);

        CHECK_NEAR (firResponseDb (h, 5657.0, fs), 0.0, 0.5);
        CHECK (firResponseDb (h, 500.0, fs) < -30.0);
        CHECK (firResponseDb (h, 1000.0, fs) < -20.0);
    });

    run ("inside + outside reconstructs the delayed input exactly", [] {
        const double fs = 48000.0;
        const int n = 8192;
        juce::AudioBuffer<float> noise (2, n);
        std::mt19937 rng (1);
        std::uniform_real_distribution<float> dist (-0.5f, 0.5f);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                noise.setSample (ch, i, dist (rng));

        auto runListen = [&] (rde::Listen l) {
            rde::DeEsserEngine e;
            e.prepare (fs, 256, 2);
            auto p = sibilanceParams();
            p.listen = l;
            auto b = noise;
            processInBlocks (e, b, p);
            return b;
        };

        const auto in = runListen (rde::Listen::inside);
        const auto out = runListen (rde::Listen::outside);
        const int lat = rde::DeEsserEngine::halfLengthFor (fs);
        double maxErr = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = lat; i < n; ++i)
                maxErr = std::max (maxErr, (double) std::abs (in.getSample (ch, i) + out.getSample (ch, i) - noise.getSample (ch, i - lat)));
        CHECK (maxErr < 1e-5);
    });

    run ("transparent below threshold, latency reported correctly", [] {
        const double fs = 48000.0;
        rde::DeEsserEngine e;
        e.prepare (fs, 512, 2);
        juce::AudioBuffer<float> b (2, 2048);
        b.clear();
        b.setSample (0, 0, 0.1f);
        b.setSample (1, 0, 0.1f);
        auto p = sibilanceParams();
        p.thresholdDb = 0.0f;
        processInBlocks (e, b, p);
        const int lat = e.getLatencySamples();
        CHECK_NEAR (b.getSample (0, lat), 0.1, 1e-6);
        double other = 0.0;
        for (int i = 0; i < 2048; ++i)
            if (i != lat)
                other = std::max (other, (double) std::abs (b.getSample (0, i)));
        CHECK (other < 1e-6);
    });

    run ("de-esses an in-band tone, leaves an out-of-band tone alone", [] {
        const double fs = 48000.0;
        const int n = 48000;

        rde::DeEsserEngine e1;
        e1.prepare (fs, 512, 2);
        auto ess = sine (fs, 6000.0, 0.5f, n);
        const double essIn = rmsDb (ess, 0, n / 2, n / 2);
        processInBlocks (e1, ess, sibilanceParams());
        CHECK (rmsDb (ess, 0, n / 2, n / 2) < essIn - 18.0);

        rde::DeEsserEngine e2;
        e2.prepare (fs, 512, 2);
        auto low = sine (fs, 300.0, 0.5f, n);
        const double lowIn = rmsDb (low, 0, n / 2, n / 2);
        processInBlocks (e2, low, sibilanceParams());
        CHECK_NEAR (rmsDb (low, 0, n / 2, n / 2), lowIn, 0.2);
    });

    run ("wide audio mode attenuates the whole signal", [] {
        const double fs = 48000.0;
        const int n = 48000;
        rde::DeEsserEngine e;
        e.prepare (fs, 512, 2);
        juce::AudioBuffer<float> b (2, n);
        auto ess = sine (fs, 6000.0, 0.5f, n);
        auto low = sine (fs, 300.0, 0.25f, n);
        b.makeCopyOf (ess);
        for (int ch = 0; ch < 2; ++ch)
            b.addFrom (ch, 0, low, ch, 0, n);
        auto p = sibilanceParams();
        p.audioWide = true;
        p.ratio = 4.0f;
        processInBlocks (e, b, p);
        CHECK (e.getStatus().grDb < -10.0f);
        // the low tone is reduced too (it would be untouched in band mode)
        CHECK (rmsDb (b, 0, n / 2, n / 2) < rmsDb (low, 0, n / 2, n / 2) - 3.0);
    });

    run ("floating threshold keeps reduction constant across levels", [] {
        const double fs = 48000.0;
        const int n = 48000 * 6;
        auto measure = [&] (float amp, bool autoTrack) {
            rde::DeEsserEngine e;
            e.prepare (fs, 512, 2);
            auto b = sine (fs, 6000.0, amp, n);
            auto p = sibilanceParams();
            p.autoTrack = autoTrack;
            p.dampingS = 0.5f;
            processInBlocks (e, b, p);
            return e.getStatus().grDb;
        };

        const float loudAuto = measure (0.5f, true), quietAuto = measure (0.05f, true);
        CHECK_NEAR (loudAuto, quietAuto, 1.0);
        CHECK (loudAuto < -5.0f);

        const float loudFixed = measure (0.5f, false), quietFixed = measure (0.05f, false);
        CHECK (quietFixed > loudFixed + 15.0f);
    });

    run ("effect off outputs the delayed input", [] {
        const double fs = 48000.0;
        const int n = 16384;
        rde::DeEsserEngine e;
        e.prepare (fs, 512, 2);
        auto src = sine (fs, 6000.0, 0.5f, n);
        auto b = src;
        auto p = sibilanceParams();
        p.effectOn = false;
        p.makeupDb = 6.0f;
        processInBlocks (e, b, p);
        const int lat = e.getLatencySamples();
        double maxErr = 0.0;
        for (int i = lat; i < n; ++i)
            maxErr = std::max (maxErr, (double) std::abs (b.getSample (1, i) - src.getSample (1, i - lat)));
        CHECK (maxErr < 1e-5);
    });

    run ("band changes are click-free (crossfaded)", [] {
        const double fs = 48000.0;
        const int n = 48000;
        rde::DeEsserEngine e;
        e.prepare (fs, 64, 1);
        auto b = sine (fs, 5000.0, 0.5f, n, 1);
        auto p = sibilanceParams();
        p.listen = rde::Listen::inside;
        float maxStep = 0.0f, prev = 0.0f;
        for (int pos = 0; pos < n; pos += 64)
        {
            p.freqHz = 5000.0f + 3000.0f * (float) std::sin (pos * 0.001);
            juce::AudioBuffer<float> view (b.getArrayOfWritePointers(), 1, pos, 64);
            e.process (view, p);
            for (int i = 0; i < 64; ++i)
            {
                if (pos + i > 2000)
                    maxStep = std::max (maxStep, std::abs (view.getSample (0, i) - prev));
                prev = view.getSample (0, i);
            }
        }
        // a 5 kHz sine at 0.5 peak moves at most ~0.33 per sample
        CHECK (maxStep < 0.4f);
    });

    std::printf ("\n%d failure(s)\n", failures);
    return failures;
}
