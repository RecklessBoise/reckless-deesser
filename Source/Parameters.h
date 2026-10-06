#pragma once

#include "dsp/DeEsserEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace rde
{
namespace ids
{
    inline constexpr const char* inTrim = "inTrim";
    inline constexpr const char* outTrim = "outTrim";
    inline constexpr const char* listen = "listen";
    inline constexpr const char* freq = "freq";
    inline constexpr const char* width = "width";
    inline constexpr const char* slope = "slope";
    inline constexpr const char* threshold = "threshold";
    inline constexpr const char* ratio = "ratio";
    inline constexpr const char* knee = "knee";
    inline constexpr const char* makeup = "makeup";
    inline constexpr const char* mix = "mix";
    inline constexpr const char* effect = "effect";
    inline constexpr const char* trigger = "trigger";
    inline constexpr const char* audio = "audio";
    inline constexpr const char* attack = "attack";
    inline constexpr const char* hold = "hold";
    inline constexpr const char* release = "release";
    inline constexpr const char* autoTrack = "autoTrack";
    inline constexpr const char* damping = "damping";
} // namespace ids

/** Ratio values at or above this read as "CUT" (infinite ratio). */
inline constexpr float kRatioCut = 99.5f;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Cached raw parameter pointers, read lock-free on the audio thread. */
class ParameterReader
{
public:
    explicit ParameterReader (juce::AudioProcessorValueTreeState& state);
    EngineParams read() const noexcept;

private:
    std::atomic<float>* get (juce::AudioProcessorValueTreeState& s, const char* id);

    std::atomic<float> *inTrim, *outTrim, *listen, *freq, *width, *slope, *threshold, *ratio, *knee,
        *makeup, *mix, *effect, *trigger, *audio, *attack, *hold, *release, *autoTrack, *damping;
};

} // namespace rde
