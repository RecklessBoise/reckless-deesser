#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
void storeMax (std::atomic<float>& target, float value) noexcept
{
    float current = target.load();
    while (value > current && ! target.compare_exchange_weak (current, value)) {}
}

void storeMin (std::atomic<float>& target, float value) noexcept
{
    float current = target.load();
    while (value < current && ! target.compare_exchange_weak (current, value)) {}
}
} // namespace

RecklessDeEsserProcessor::RecklessDeEsserProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RecklessDeEsser", rde::createParameterLayout()),
      params (apvts)
{
}

bool RecklessDeEsserProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void RecklessDeEsserProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = std::max (1, getTotalNumOutputChannels());
    engine.prepare (sampleRate, std::max (32, samplesPerBlock), channels);
    monoScratch.assign ((size_t) std::max (32, samplesPerBlock), 0.0f);
    currentSampleRate = sampleRate;
    setLatencySamples (engine.getLatencySamples());
}

void RecklessDeEsserProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.process (buffer, params.read(), this);

    const auto& status = engine.getStatus();
    storeMin (gainReductionDb, status.grDb);
    effectiveThresholdDb = status.thresholdDb;
}

void RecklessDeEsserProcessor::onChunk (const float* const* delayedInput, const float* const* output,
                                        int numChannels, int numSamples)
{
    const float norm = 1.0f / (float) numChannels;
    const int n = std::min (numSamples, (int) monoScratch.size());

    auto feed = [&] (const float* const* src, rde::AnalyserFifo& fifo, std::atomic<float>& peak) {
        float pk = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float sum = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                sum += src[ch][i];
                pk = std::max (pk, std::abs (src[ch][i]));
            }

            monoScratch[(size_t) i] = sum * norm;
        }

        fifo.push (monoScratch.data(), n);
        storeMax (peak, pk);
    };

    feed (delayedInput, inputFifo, inputPeak);
    feed (output, outputFifo, outputPeak);
}

juce::AudioProcessorEditor* RecklessDeEsserProcessor::createEditor()
{
    return new RecklessDeEsserEditor (*this);
}

void RecklessDeEsserProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiScale", uiScale.load(), nullptr);
    state.setProperty ("advancedOpen", advancedOpen.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RecklessDeEsserProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto state = juce::ValueTree::fromXml (*xml);
    uiScale = juce::jlimit (0.5f, 2.5f, (float) state.getProperty ("uiScale", 1.0f));
    advancedOpen = (bool) state.getProperty ("advancedOpen", true);
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RecklessDeEsserProcessor();
}
