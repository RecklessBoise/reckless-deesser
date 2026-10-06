#pragma once

#include "Parameters.h"
#include "dsp/AnalyserFifo.h"
#include "dsp/DeEsserEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

class RecklessDeEsserProcessor final : public juce::AudioProcessor,
                                       private rde::DeEsserEngine::Tap
{
public:
    RecklessDeEsserProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    /** Audio -> UI data. Peaks and reduction are "max since last read". */
    rde::AnalyserFifo inputFifo, outputFifo;
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gainReductionDb { 0.0f };
    std::atomic<float> effectiveThresholdDb { -30.0f };
    std::atomic<double> currentSampleRate { 48000.0 };

    /** Editor state, saved with the session. */
    std::atomic<float> uiScale { 1.0f };
    std::atomic<bool> advancedOpen { true };

private:
    void onChunk (const float* const* delayedInput, const float* const* output, int numChannels, int numSamples) override;

    rde::ParameterReader params;
    rde::DeEsserEngine engine;
    std::vector<float> monoScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessDeEsserProcessor)
};
