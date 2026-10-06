#pragma once

#include "BandSplitter.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <limits>

namespace rde
{

enum class Listen { mix = 0, inside, outside };

struct EngineParams
{
    float inTrimDb = 0.0f, outTrimDb = 0.0f;
    float freqHz = 6000.0f, widthOct = 1.5f, slopeDbPerOct = 48.0f;
    float thresholdDb = -30.0f, ratio = 4.0f, kneeDb = 5.0f; // ratio = +inf -> "cut"
    float makeupDb = 0.0f, mix = 1.0f;
    float attackMs = 1.0f, holdS = 0.01f, releaseS = 0.1f, dampingS = 1.0f;
    bool triggerWide = false, audioWide = false, autoTrack = true, effectOn = true;
    Listen listen = Listen::mix;
};

struct EngineStatus
{
    float grDb = 0.0f;          // most negative gain change over the last process() call
    float envDb = -120.0f;      // detector level
    float thresholdDb = -30.0f; // effective (floating) threshold
    float trackDb = -20.0f;     // tracked programme level
};

/** Static gain curve: returns gain change in dB (<= 0) for a level overDb above threshold. */
inline float gainReductionDb (float overDb, float ratio, float kneeDb) noexcept
{
    const float slope = std::isinf (ratio) ? -1.0f : (1.0f / std::max (1.0f, ratio) - 1.0f);

    if (kneeDb > 0.0f && std::abs (2.0f * overDb) < kneeDb)
    {
        const float x = overDb + 0.5f * kneeDb;
        return slope * x * x / (2.0f * kneeDb);
    }

    return overDb > 0.0f ? slope * overDb : 0.0f;
}

class DeEsserEngine
{
public:
    static constexpr float kTrackingReferenceDb = -20.0f; // RMS level at which the threshold reads "as set"
    static constexpr double kTargetLatencySeconds = 0.00195;

    /** Receives each processed chunk (delayed, trimmed input and final output). */
    struct Tap
    {
        virtual ~Tap() = default;
        virtual void onChunk (const float* const* delayedInput, const float* const* output, int numChannels, int numSamples) = 0;
    };

    static int halfLengthFor (double sampleRate) noexcept
    {
        return std::max (16, (int) std::lround (kTargetLatencySeconds * sampleRate));
    }

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        fs = sampleRate;
        maxBlock = std::max (1, maxBlockSize);
        channels = std::max (1, numChannels);

        splitter.prepare (fs, halfLengthFor (fs), channels);
        inside.setSize (channels, maxBlock);
        delayed.setSize (channels, maxBlock);

        for (auto* v : { &gainScratch, &inTrimScratch, &makeupScratch, &mixScratch, &outTrimScratch, &effectScratch })
            v->assign ((size_t) maxBlock, 0.0f);

        for (auto* s : { &inTrim, &makeup, &mixAmt, &outTrim, &effect })
            s->reset (fs, 0.02);

        firstBlock = true;
        lastLo = lastHi = lastSlope = -1.0;
        reset();
    }

    void reset()
    {
        splitter.reset();
        env = -120.0f;
        holdCount = 0;
        trackPow = std::pow (10.0f, kTrackingReferenceDb * 0.1f); // power domain
    }

    int getLatencySamples() const noexcept { return splitter.getLatency(); }
    const EngineStatus& getStatus() const noexcept { return status; }
    const BandSplitter& getSplitter() const noexcept { return splitter; }

    void process (juce::AudioBuffer<float>& buffer, const EngineParams& p, Tap* tap = nullptr)
    {
        const int numCh = std::min (channels, buffer.getNumChannels());
        const int total = buffer.getNumSamples();

        updateTargets (p);
        status.grDb = 0.0f;

        for (int pos = 0; pos < total;)
        {
            const int n = std::min (maxBlock, total - pos);
            processChunk (buffer, numCh, pos, n, p, tap);
            pos += n;
        }
    }

private:
    void updateTargets (const EngineParams& p)
    {
        const float effectTarget = p.effectOn ? 1.0f : 0.0f;
        const float in = juce::Decibels::decibelsToGain (p.inTrimDb);
        const float mu = juce::Decibels::decibelsToGain (p.makeupDb);
        const float out = juce::Decibels::decibelsToGain (p.outTrimDb);
        const float mx = juce::jlimit (0.0f, 1.0f, p.mix);

        if (firstBlock)
        {
            inTrim.setCurrentAndTargetValue (in);
            makeup.setCurrentAndTargetValue (mu);
            mixAmt.setCurrentAndTargetValue (mx);
            outTrim.setCurrentAndTargetValue (out);
            effect.setCurrentAndTargetValue (effectTarget);
            firstBlock = false;
        }
        else
        {
            inTrim.setTargetValue (in);
            makeup.setTargetValue (mu);
            mixAmt.setTargetValue (mx);
            outTrim.setTargetValue (out);
            effect.setTargetValue (effectTarget);
        }

        double lo = 0.0, hi = 0.0;
        bandEdges (p.freqHz, p.widthOct, fs, lo, hi);

        if (std::abs (lo - lastLo) > lastLo * 1.0e-4 || std::abs (hi - lastHi) > lastHi * 1.0e-4
            || std::abs (p.slopeDbPerOct - lastSlope) > 1.0e-3)
        {
            splitter.setBand (lo, hi, p.slopeDbPerOct);
            lastLo = lo;
            lastHi = hi;
            lastSlope = p.slopeDbPerOct;
        }
    }

    static void fillSmoothed (juce::SmoothedValue<float>& s, std::vector<float>& dest, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            dest[(size_t) i] = s.getNextValue();
    }

    void processChunk (juce::AudioBuffer<float>& buffer, int numCh, int start, int n, const EngineParams& p, Tap* tap)
    {
        fillSmoothed (inTrim, inTrimScratch, n);
        fillSmoothed (makeup, makeupScratch, n);
        fillSmoothed (mixAmt, mixScratch, n);
        fillSmoothed (outTrim, outTrimScratch, n);
        fillSmoothed (effect, effectScratch, n);

        // 1. Input trim, then split into inside band + latency-matched dry.
        splitter.beginBlock();

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* io = buffer.getWritePointer (ch, start);

            for (int i = 0; i < n; ++i)
                io[i] *= inTrimScratch[(size_t) i];

            splitter.processChannel (ch, io, inside.getWritePointer (ch), delayed.getWritePointer (ch), n);
        }

        splitter.endBlock (n);

        // 2. Detection, floating threshold and gain computer (linked across channels).
        const float aAtt = std::exp (-1.0f / (std::max (0.01f, p.attackMs) * 0.001f * (float) fs));
        const float aRel = std::exp (-1.0f / (std::max (0.001f, p.releaseS) * (float) fs));
        const float aTrk = std::exp (-1.0f / (std::max (0.01f, p.dampingS) * (float) fs));
        const int holdSamples = (int) (std::max (0.0f, p.holdS) * (float) fs);
        constexpr float gatePow = 1.0e-7f; // -70 dBFS: stop tracking in silence
        const float invCh = 1.0f / (float) numCh;

        float minGr = 0.0f, thr = p.thresholdDb;

        for (int i = 0; i < n; ++i)
        {
            float trig = 0.0f, pow = 0.0f;

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float d = delayed.getSample (ch, i);
                const float s = p.triggerWide ? d : inside.getSample (ch, i);
                trig = std::max (trig, std::abs (s));
                pow += d * d;
            }

            pow *= invCh;

            if (pow > gatePow)
                trackPow = pow + aTrk * (trackPow - pow);

            const float lvl = trig > 1.0e-6f ? 20.0f * std::log10 (trig) : -120.0f;

            if (lvl > env)
            {
                env = lvl + aAtt * (env - lvl);
                holdCount = holdSamples;
            }
            else if (holdCount > 0)
            {
                --holdCount;
            }
            else
            {
                env = lvl + aRel * (env - lvl);
            }

            thr = p.thresholdDb;

            if (p.autoTrack)
                thr += juce::jlimit (-40.0f, 30.0f, 10.0f * std::log10 (trackPow + 1.0e-12f) - kTrackingReferenceDb);

            const float gr = gainReductionDb (env - thr, p.ratio, p.kneeDb);
            minGr = std::min (minGr, gr);
            gainScratch[(size_t) i] = std::pow (10.0f, gr * 0.05f);
        }

        // 3. Apply gain, listen modes, wet/dry, bypass and output trim.
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* io = buffer.getWritePointer (ch, start);
            const float* d = delayed.getReadPointer (ch);
            const float* in = inside.getReadPointer (ch);

            for (int i = 0; i < n; ++i)
            {
                float y;

                switch (p.listen)
                {
                    case Listen::inside:  y = in[i]; break;
                    case Listen::outside: y = d[i] - in[i]; break;
                    case Listen::mix:
                    default:
                    {
                        const float g = gainScratch[(size_t) i];
                        float wet = p.audioWide ? g * d[i] : d[i] - (1.0f - g) * in[i];
                        wet *= makeupScratch[(size_t) i];
                        const float m = mixScratch[(size_t) i];
                        y = m * wet + (1.0f - m) * d[i];
                        break;
                    }
                }

                const float e = effectScratch[(size_t) i];
                y = e * y + (1.0f - e) * d[i];
                io[i] = y * outTrimScratch[(size_t) i];
            }
        }

        // Channels the engine wasn't prepared for are silenced rather than left unprocessed.
        for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
            buffer.clear (ch, start, n);

        status.grDb = std::min (status.grDb, minGr);
        status.envDb = env;
        status.thresholdDb = thr;
        status.trackDb = 10.0f * std::log10 (trackPow + 1.0e-12f);

        if (tap != nullptr)
        {
            const float* outPtrs[8] {};
            const int tapCh = std::min (numCh, 8);

            for (int ch = 0; ch < tapCh; ++ch)
                outPtrs[ch] = buffer.getReadPointer (ch, start);

            tap->onChunk (delayed.getArrayOfReadPointers(), outPtrs, tapCh, n);
        }
    }

    BandSplitter splitter;
    juce::AudioBuffer<float> inside, delayed;
    std::vector<float> gainScratch, inTrimScratch, makeupScratch, mixScratch, outTrimScratch, effectScratch;
    juce::SmoothedValue<float> inTrim, makeup, mixAmt, outTrim, effect;
    EngineStatus status;
    double fs = 48000.0, lastLo = -1.0, lastHi = -1.0, lastSlope = -1.0;
    int maxBlock = 512, channels = 2, holdCount = 0;
    float env = -120.0f, trackPow = 0.01f;
    bool firstBlock = true;
};

} // namespace rde
