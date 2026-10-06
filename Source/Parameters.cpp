#include "Parameters.h"

namespace rde
{
namespace
{
    using Fmt = std::function<juce::String (float, int)>;
    using Parse = std::function<float (const juce::String&)>;

    juce::String signedDb (float v, int decimals)
    {
        return (v > 0.0f ? "+" : "") + juce::String (v, decimals) + " dB";
    }

    float parseNumber (const juce::String& t)
    {
        auto v = t.retainCharacters ("0123456789.-+").getFloatValue();
        if (t.containsIgnoreCase ("k"))
            v *= 1000.0f;
        return v;
    }

    juce::String seconds (float v)
    {
        return juce::String (v, v < 0.0995f ? 2 : (v < 0.9995f ? 3 : 1)) + " s";
    }

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const char* name,
                                                          juce::NormalisableRange<float> range, float def,
                                                          Fmt fmt, Parse parse = parseNumber)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt)).withValueFromStringFunction (std::move (parse)));
    }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto trimFmt = [] (float v, int) { return signedDb (v, 2); };

    layout.add (makeFloat (ids::inTrim, "Input Trim", { -24.0f, 24.0f }, 0.0f, trimFmt));
    layout.add (makeFloat (ids::outTrim, "Output Trim", { -24.0f, 24.0f }, 0.0f, trimFmt));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::listen, 1 }, "Listen", juce::StringArray { "Mix", "Inside", "Outside" }, 0));

    layout.add (makeFloat (ids::freq, "Frequency", skewed (1000.0f, 16000.0f, 4000.0f), 6000.0f,
                           [] (float v, int) {
                               return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz"
                                                  : juce::String (v / 1000.0f, 2) + " kHz";
                           }));
    layout.add (makeFloat (ids::width, "Width", skewed (0.1f, 5.0f, 1.5f), 1.5f,
                           [] (float v, int) { return juce::String (v, 2); }));
    layout.add (makeFloat (ids::slope, "Slope", { 6.0f, 96.0f }, 48.0f,
                           [] (float v, int) { return juce::String (juce::roundToInt (v)) + " dB"; }));

    layout.add (makeFloat (ids::threshold, "Threshold", { -95.0f, 0.0f }, -30.0f,
                           [] (float v, int) { return juce::String (v, 1) + " dB"; }));
    layout.add (makeFloat (ids::ratio, "Ratio", skewed (1.0f, 100.0f, 8.0f), 4.0f,
                           [] (float v, int) { return v >= kRatioCut ? juce::String ("CUT") : juce::String (v, 1) + " :1"; },
                           [] (const juce::String& t) { return t.containsIgnoreCase ("cut") ? 100.0f : parseNumber (t); }));
    layout.add (makeFloat (ids::knee, "Soft Knee", { 0.0f, 20.0f }, 5.0f,
                           [] (float v, int) { return juce::String (v, v < 10.0f ? 1 : 0) + " dB"; }));
    layout.add (makeFloat (ids::makeup, "Make-Up", { -12.0f, 24.0f }, 0.0f, trimFmt));
    layout.add (makeFloat (ids::mix, "Wet/Dry", { 0.0f, 100.0f }, 100.0f,
                           [] (float v, int) { return juce::String (v, 1) + " %"; }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::effect, 1 }, "Effect", true,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool b, int) { return b ? "IN" : "OUT"; })));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::trigger, 1 }, "Trigger", juce::StringArray { "Band", "Wide" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ids::audio, 1 }, "Audio", juce::StringArray { "Band", "Wide" }, 0));

    layout.add (makeFloat (ids::attack, "Attack", skewed (0.05f, 50.0f, 2.0f), 1.0f,
                           [] (float v, int) { return juce::String (v, 2) + " ms"; }));
    layout.add (makeFloat (ids::hold, "Hold", skewed (0.0f, 1.0f, 0.05f), 0.01f,
                           [] (float v, int) { return seconds (v); }));
    layout.add (makeFloat (ids::release, "Release", skewed (0.005f, 2.0f, 0.15f), 0.1f,
                           [] (float v, int) { return seconds (v); }));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ids::autoTrack, 1 }, "Level Tracking", true,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction ([] (bool b, int) { return b ? "IN" : "OUT"; })));
    layout.add (makeFloat (ids::damping, "Damping", skewed (0.1f, 10.0f, 1.0f), 1.0f,
                           [] (float v, int) { return seconds (v); }));

    return layout;
}

std::atomic<float>* ParameterReader::get (juce::AudioProcessorValueTreeState& s, const char* id)
{
    auto* p = s.getRawParameterValue (id);
    jassert (p != nullptr);
    return p;
}

ParameterReader::ParameterReader (juce::AudioProcessorValueTreeState& s)
    : inTrim (get (s, ids::inTrim)), outTrim (get (s, ids::outTrim)), listen (get (s, ids::listen)),
      freq (get (s, ids::freq)), width (get (s, ids::width)), slope (get (s, ids::slope)),
      threshold (get (s, ids::threshold)), ratio (get (s, ids::ratio)), knee (get (s, ids::knee)),
      makeup (get (s, ids::makeup)), mix (get (s, ids::mix)), effect (get (s, ids::effect)),
      trigger (get (s, ids::trigger)), audio (get (s, ids::audio)), attack (get (s, ids::attack)),
      hold (get (s, ids::hold)), release (get (s, ids::release)), autoTrack (get (s, ids::autoTrack)),
      damping (get (s, ids::damping))
{
}

EngineParams ParameterReader::read() const noexcept
{
    EngineParams p;
    p.inTrimDb = inTrim->load();
    p.outTrimDb = outTrim->load();
    p.listen = (Listen) juce::jlimit (0, 2, juce::roundToInt (listen->load()));
    p.freqHz = freq->load();
    p.widthOct = width->load();
    p.slopeDbPerOct = slope->load();
    p.thresholdDb = threshold->load();
    const float r = ratio->load();
    p.ratio = r >= kRatioCut ? std::numeric_limits<float>::infinity() : r;
    p.kneeDb = knee->load();
    p.makeupDb = makeup->load();
    p.mix = mix->load() * 0.01f;
    p.effectOn = effect->load() > 0.5f;
    p.triggerWide = trigger->load() > 0.5f;
    p.audioWide = audio->load() > 0.5f;
    p.attackMs = attack->load();
    p.holdS = hold->load();
    p.releaseS = release->load();
    p.autoTrack = autoTrack->load() > 0.5f;
    p.dampingS = damping->load();
    return p;
}

} // namespace rde
