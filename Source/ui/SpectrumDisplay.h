#pragma once

#include "Theme.h"
#include "dsp/AnalyserFifo.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class RecklessDeEsserProcessor;

namespace rde::ui
{

/** FFT magnitude of the most recent samples pulled from an AnalyserFifo (UI thread). */
class SpectrumAnalyser
{
public:
    static constexpr int order = 12;
    static constexpr int size = 1 << order;
    static constexpr int numBins = size / 2 + 1;

    SpectrumAnalyser();
    void pull (AnalyserFifo& fifo);
    void compute();
    float binDb (int bin) const noexcept { return smoothedDb[(size_t) bin]; }

private:
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::hann, false };
    std::vector<float> ring, fftData, pullScratch, smoothedDb;
    int writePos = 0;
};

/** The central analyser: spectrum, band handles, floating threshold and reduction read-outs. */
class SpectrumDisplay : public juce::Component
{
public:
    explicit SpectrumDisplay (RecklessDeEsserProcessor&);

    void refresh (float grDb, float effectiveThresholdDb); // ~30 Hz from the editor timer
    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class Drag { none, low, high, centre, threshold, scroll };

    float xForFreq (double hz) const;
    double freqForX (float x) const;
    float yForDb (float db) const;
    float dbForY (float y) const;
    void bandEdges (double& lo, double& hi) const;
    Drag hitTestDrag (juce::Point<float>) const;
    void setParam (juce::RangedAudioParameter&, float value);
    void setBandFromEdges (double lo, double hi);
    void clampView();

    void drawGrid (juce::Graphics&);
    void drawSpectrum (juce::Graphics&, const SpectrumAnalyser&, juce::Colour);
    void drawBand (juce::Graphics&);
    void drawThreshold (juce::Graphics&);
    void drawPeak (juce::Graphics&);
    void drawScrollBar (juce::Graphics&);

    RecklessDeEsserProcessor& processor;
    juce::RangedAudioParameter &freq, &width, &threshold, &knee;
    SpectrumAnalyser inputSpectrum, outputSpectrum;

    juce::Rectangle<float> plot, scrollTrack, zoomButton;
    static constexpr double fullLoOct = 4.6439, fullHiOct = 14.2877; // 25 Hz .. 20 kHz
    static constexpr double zoomSpanOct = 5.0;
    double viewLoOct = fullLoOct, viewSpanOct = fullHiOct - fullLoOct;
    bool zoomed = false;

    float grDb = 0.0f, thresholdDb = -30.0f;
    Drag drag = Drag::none;
    float dragStartValue = 0.0f, dragStartY = 0.0f;
    double dragStartViewLo = 0.0, dragOtherEdge = 0.0;
};

} // namespace rde::ui
