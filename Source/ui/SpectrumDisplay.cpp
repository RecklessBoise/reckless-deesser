#include "SpectrumDisplay.h"

#include "Parameters.h"
#include "PluginProcessor.h"

namespace rde::ui
{

//==============================================================================
SpectrumAnalyser::SpectrumAnalyser()
    : ring ((size_t) size, 0.0f), fftData ((size_t) (2 * size), 0.0f), pullScratch ((size_t) size, 0.0f),
      smoothedDb ((size_t) numBins, -120.0f)
{
}

void SpectrumAnalyser::pull (AnalyserFifo& fifo)
{
    for (;;)
    {
        const int n = fifo.pull (pullScratch.data(), size);

        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) writePos] = pullScratch[(size_t) i];
            writePos = (writePos + 1) % size;
        }

        if (n < size)
            break;
    }
}

void SpectrumAnalyser::compute()
{
    for (int i = 0; i < size; ++i)
        fftData[(size_t) i] = ring[(size_t) ((writePos + i) % size)];

    std::fill (fftData.begin() + size, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) size);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    // Hann coherent gain is 0.5: a full-scale sine reads 0 dB.
    const float norm = 4.0f / (float) size;

    for (int b = 0; b < numBins; ++b)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[(size_t) b] * norm, -120.0f);
        auto& s = smoothedDb[(size_t) b];
        s = db > s ? db : s + 0.35f * (db - s);
    }
}

//==============================================================================
namespace
{
    constexpr float topDb = 0.0f, bottomDb = -95.0f;

    juce::String hzText (double hz)
    {
        return hz < 1000.0 ? juce::String (juce::roundToInt (hz)) + "Hz" : juce::String (hz / 1000.0, 2) + "kHz";
    }

    juce::RangedAudioParameter& param (RecklessDeEsserProcessor& p, const char* id)
    {
        auto* r = p.apvts.getParameter (id);
        jassert (r != nullptr);
        return *r;
    }
} // namespace

SpectrumDisplay::SpectrumDisplay (RecklessDeEsserProcessor& p)
    : processor (p), freq (param (p, ids::freq)), width (param (p, ids::width)),
      threshold (param (p, ids::threshold)), knee (param (p, ids::knee))
{
    setOpaque (true);
}

void SpectrumDisplay::resized()
{
    const auto b = getLocalBounds().toFloat();
    plot = { 36.0f, 24.0f, b.getWidth() - 36.0f - 23.0f, b.getHeight() - 24.0f - 47.0f };
    scrollTrack = { 3.0f, plot.getBottom() + 25.0f, b.getWidth() - 36.0f, 14.0f };
    zoomButton = { scrollTrack.getRight() + 7.0f, scrollTrack.getY() - 3.0f, 20.0f, 20.0f };
}

void SpectrumDisplay::refresh (float newGrDb, float newThresholdDb)
{
    grDb = newGrDb;
    thresholdDb = newThresholdDb;
    inputSpectrum.pull (processor.inputFifo);
    outputSpectrum.pull (processor.outputFifo);
    inputSpectrum.compute();
    outputSpectrum.compute();
    repaint();
}

//==============================================================================
float SpectrumDisplay::xForFreq (double hz) const
{
    return plot.getX() + (float) ((std::log2 (std::max (hz, 1.0)) - viewLoOct) / viewSpanOct) * plot.getWidth();
}

double SpectrumDisplay::freqForX (float x) const
{
    return std::exp2 (viewLoOct + (double) ((x - plot.getX()) / plot.getWidth()) * viewSpanOct);
}

float SpectrumDisplay::yForDb (float db) const
{
    return plot.getY() + (topDb - db) / (topDb - bottomDb) * plot.getHeight();
}

float SpectrumDisplay::dbForY (float y) const
{
    return topDb - (y - plot.getY()) / plot.getHeight() * (topDb - bottomDb);
}

void SpectrumDisplay::bandEdges (double& lo, double& hi) const
{
    rde::bandEdges (freq.convertFrom0to1 (freq.getValue()), width.convertFrom0to1 (width.getValue()),
                    processor.currentSampleRate.load(), lo, hi);
}

void SpectrumDisplay::clampView()
{
    viewLoOct = juce::jlimit (fullLoOct, fullHiOct - viewSpanOct, viewLoOct);
}

//==============================================================================
void SpectrumDisplay::paint (juce::Graphics& g)
{
    g.fillAll (theme::displayBg);

    juce::ColourGradient bg (theme::displayBg2, plot.getX(), plot.getY(), theme::displayBg, plot.getX(), plot.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRect (plot);

    drawGrid (g);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (plot.toNearestInt());
        drawSpectrum (g, inputSpectrum, theme::spectrumIn);
        drawSpectrum (g, outputSpectrum, theme::spectrumOut);
        drawPeak (g);
        drawThreshold (g);
    }

    drawBand (g);
    drawScrollBar (g);

    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRect (plot.expanded (1.0f), 1.0f);
}

void SpectrumDisplay::drawGrid (juce::Graphics& g)
{
    g.setFont (theme::font (9.5f));

    for (int db = 0; db >= -95; db -= 5)
    {
        const float y = yForDb ((float) db);
        g.setColour (theme::grid.withAlpha (db % 10 == 0 ? 0.9f : 0.55f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

        g.setColour (theme::gridText);
        const auto text = juce::String (db);
        g.drawText (text, juce::Rectangle<float> (0.0f, y - 6.0f, plot.getX() - 5.0f, 12.0f), juce::Justification::centredRight, false);
        g.drawText (text, juce::Rectangle<float> (plot.getRight() + 3.0f, y - 6.0f, 22.0f, 12.0f), juce::Justification::centredLeft, false);
    }

    static constexpr double ticks[] = { 20, 30, 40, 60, 80, 100, 200, 300, 400, 600, 800, 1000, 2000, 3000, 4000, 6000, 8000, 10000, 15000, 20000 };
    float lastLabelX = -1000.0f;

    for (double f : ticks)
    {
        const float x = xForFreq (f);
        if (x < plot.getX() || x > plot.getRight())
            continue;

        g.setColour (theme::grid.withAlpha (0.8f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());

        if (x - lastLabelX < 24.0f)
            continue;

        lastLabelX = x;
        const auto text = f >= 1000.0 ? juce::String (f / 1000.0, 0) + "K" : juce::String ((int) f);
        g.setColour (theme::gridText);
        g.drawText (text, juce::Rectangle<float> (x - 18.0f, plot.getBottom() + 3.0f, 36.0f, 14.0f), juce::Justification::centred, false);
    }
}

void SpectrumDisplay::drawSpectrum (juce::Graphics& g, const SpectrumAnalyser& spectrum, juce::Colour colour)
{
    const double fs = processor.currentSampleRate.load();
    const double binHz = fs / SpectrumAnalyser::size;
    juce::Path path;
    path.startNewSubPath (plot.getX(), plot.getBottom());

    for (float x = plot.getX(); x < plot.getRight(); x += 1.5f)
    {
        const int b0 = juce::jlimit (1, SpectrumAnalyser::numBins - 1, (int) (freqForX (x) / binHz));
        const int b1 = juce::jlimit (b0, SpectrumAnalyser::numBins - 1, (int) (freqForX (x + 1.5f) / binHz));
        float db = -120.0f;

        for (int b = b0; b <= b1; ++b)
            db = std::max (db, spectrum.binDb (b));

        const float y = juce::jlimit (plot.getY(), plot.getBottom(), yForDb (db));
        path.lineTo (x, y);
        path.lineTo (x + 1.5f, y);
    }

    path.lineTo (plot.getRight(), plot.getBottom());
    path.closeSubPath();

    juce::ColourGradient grad (colour.brighter (0.3f).withAlpha (0.95f), 0.0f, plot.getY(),
                               colour.withAlpha (0.8f), 0.0f, plot.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (path);
}

void SpectrumDisplay::drawPeak (juce::Graphics& g)
{
    const double fs = processor.currentSampleRate.load();
    const double binHz = fs / SpectrumAnalyser::size;
    int best = -1;
    float bestDb = -90.0f;

    for (int b = 2; b < SpectrumAnalyser::numBins; ++b)
    {
        const double f = b * binHz;
        if (xForFreq (f) < plot.getX() || xForFreq (f) > plot.getRight())
            continue;

        if (inputSpectrum.binDb (b) > bestDb)
        {
            bestDb = inputSpectrum.binDb (b);
            best = b;
        }
    }

    if (best < 0)
        return;

    const float x = xForFreq (best * binHz);
    g.setColour (theme::red.withAlpha (0.9f));
    g.drawLine (x, yForDb (bestDb), x, plot.getBottom() - 16.0f, 1.2f);

    const auto label = juce::String (juce::roundToInt (best * binHz)) + " Hz";
    auto box = juce::Rectangle<float> (x - 26.0f, plot.getBottom() - 16.0f, 52.0f, 13.0f);
    box.setX (juce::jlimit (plot.getX(), plot.getRight() - box.getWidth(), box.getX()));
    g.setColour (theme::displayBg.withAlpha (0.85f));
    g.fillRect (box);
    g.setColour (theme::label);
    g.setFont (theme::font (9.5f));
    g.drawText (label, box, juce::Justification::centred, false);
}

void SpectrumDisplay::drawThreshold (juce::Graphics& g)
{
    const float k = knee.convertFrom0to1 (knee.getValue());
    const float y = yForDb (thresholdDb);

    if (k > 0.0f)
    {
        const float yTop = yForDb (thresholdDb + 0.5f * k), yBot = yForDb (thresholdDb - 0.5f * k);
        g.setColour (theme::red.withAlpha (0.28f));
        g.fillRect (juce::Rectangle<float> (plot.getX(), yTop, plot.getWidth(), yBot - yTop));
        g.setColour (theme::red.withAlpha (0.8f));
        g.drawHorizontalLine (juce::roundToInt (yBot), plot.getX(), plot.getRight());
    }

    juce::Path line;
    line.startNewSubPath (plot.getX(), y);
    line.lineTo (plot.getRight(), y);
    juce::Path dashed;
    const float dashes[] = { 6.0f, 4.0f };
    juce::PathStrokeType (1.3f).createDashedStroke (dashed, line, dashes, 2);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillPath (dashed);

    g.setFont (theme::font (11.5f, true));
    g.setColour (theme::yellow);
    const auto labelArea = juce::Rectangle<float> (plot.getRight() - 90.0f, y - 15.0f, 86.0f, 13.0f);
    g.drawText ("THRESHOLD", labelArea, juce::Justification::centredRight, false);

    const auto valueArea = juce::Rectangle<float> (plot.getRight() - 64.0f, y + 3.0f, 60.0f, 15.0f);
    g.setColour (theme::displayBg.withAlpha (0.9f));
    g.fillRect (valueArea);
    g.setColour (theme::yellow);
    g.drawText (juce::String (thresholdDb, 1) + " dB", valueArea, juce::Justification::centred, false);
}

void SpectrumDisplay::drawBand (juce::Graphics& g)
{
    double lo, hi;
    bandEdges (lo, hi);
    const float xl = xForFreq (lo), xh = xForFreq (hi);
    const float xc = 0.5f * (xl + xh);

    g.setFont (theme::font (9.5f));

    for (auto [x, hz] : { std::pair { xl, lo }, std::pair { xh, hi } })
    {
        if (x < plot.getX() - 1.0f || x > plot.getRight() + 1.0f)
            continue;

        g.setColour (theme::cyan.withAlpha (0.9f));
        g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.4f);

        g.setColour (theme::label);
        g.drawText (hzText (hz), juce::Rectangle<float> (x - 28.0f, 0.0f, 56.0f, 12.0f), juce::Justification::centred, false);

        // "++" handle
        g.setColour (theme::yellow);
        for (float dx : { -3.5f, 3.5f })
        {
            g.drawLine (x + dx - 2.5f, 17.0f, x + dx + 2.5f, 17.0f, 1.2f);
            g.drawLine (x + dx, 14.5f, x + dx, 19.5f, 1.2f);
        }
    }

    // centre "hourglass" handle
    if (xc >= plot.getX() && xc <= plot.getRight())
    {
        juce::Path hg;
        hg.addTriangle (xc - 5.0f, 7.0f, xc + 5.0f, 7.0f, xc, 13.0f);
        hg.addTriangle (xc - 5.0f, 19.0f, xc + 5.0f, 19.0f, xc, 13.0f);
        g.setColour (theme::yellow);
        g.fillPath (hg);
    }

    // current reduction, drawn inside the band on the dB scale
    if (grDb < -0.05f)
    {
        const float y = yForDb (grDb);
        const float a = std::max (xl, plot.getX()), b = std::min (xh, plot.getRight());
        g.setColour (theme::red);
        g.drawLine (a, y, b, y, 1.6f);
        g.setFont (theme::font (10.0f, true));
        g.drawText (juce::String (grDb, 1) + " dB", juce::Rectangle<float> (0.5f * (a + b) - 30.0f, y - 14.0f, 60.0f, 12.0f),
                    juce::Justification::centred, false);
    }
}

void SpectrumDisplay::drawScrollBar (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (scrollTrack.withSizeKeepingCentre (scrollTrack.getWidth(), 4.0f), 2.0f);

    const double total = fullHiOct - fullLoOct;
    const float tx = scrollTrack.getX() + (float) ((viewLoOct - fullLoOct) / total) * scrollTrack.getWidth();
    const float tw = std::max (30.0f, (float) (viewSpanOct / total) * scrollTrack.getWidth());
    auto thumb = juce::Rectangle<float> (tx, scrollTrack.getY(), std::min (tw, scrollTrack.getRight() - tx), scrollTrack.getHeight());

    juce::ColourGradient grad (juce::Colour (0xffeef0f5), 0.0f, thumb.getY(), juce::Colour (0xff8e95a8), 0.0f, thumb.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (thumb, 2.0f);
    g.setColour (juce::Colour (0xff3a4058));
    g.drawRoundedRectangle (thumb, 2.0f, 1.0f);

    for (float d : { -3.0f, 0.0f, 3.0f })
        g.drawVerticalLine (juce::roundToInt (thumb.getCentreX() + d), thumb.getY() + 3.0f, thumb.getBottom() - 3.0f);

    // magnifier
    auto z = zoomButton.reduced (2.0f);
    g.setColour (zoomed ? theme::buttonOnBlue : juce::Colour (0xffd5d9e3));
    g.fillRoundedRectangle (z, 3.0f);
    g.setColour (juce::Colour (0xff1a2040));
    auto lens = z.reduced (5.0f).withTrimmedRight (2.0f).withTrimmedBottom (2.0f);
    g.drawEllipse (lens, 1.4f);
    g.drawLine (lens.getRight() - 1.0f, lens.getBottom() - 1.0f, z.getRight() - 4.0f, z.getBottom() - 4.0f, 1.8f);
}

//==============================================================================
SpectrumDisplay::Drag SpectrumDisplay::hitTestDrag (juce::Point<float> p) const
{
    if (scrollTrack.expanded (0.0f, 3.0f).contains (p))
        return Drag::scroll;

    if (! plot.withTop (0.0f).contains (p))
        return Drag::none;

    double lo, hi;
    bandEdges (lo, hi);
    const float xl = xForFreq (lo), xh = xForFreq (hi);

    if (std::abs (p.x - xl) < 5.0f)
        return Drag::low;
    if (std::abs (p.x - xh) < 5.0f)
        return Drag::high;
    if (std::abs (p.y - yForDb (thresholdDb)) < 5.0f)
        return Drag::threshold;
    if (p.x > xl && p.x < xh)
        return Drag::centre;

    return Drag::none;
}

void SpectrumDisplay::mouseMove (const juce::MouseEvent& e)
{
    switch (hitTestDrag (e.position))
    {
        case Drag::low:
        case Drag::high:      setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case Drag::threshold: setMouseCursor (juce::MouseCursor::UpDownResizeCursor); break;
        case Drag::centre:
        case Drag::scroll:    setMouseCursor (juce::MouseCursor::DraggingHandCursor); break;
        case Drag::none:      setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

void SpectrumDisplay::setParam (juce::RangedAudioParameter& p, float value)
{
    p.setValueNotifyingHost (p.convertTo0to1 (p.getNormalisableRange().snapToLegalValue (value)));
}

void SpectrumDisplay::setBandFromEdges (double lo, double hi)
{
    const double w = juce::jlimit (0.1, 5.0, std::log2 (hi / lo));
    setParam (width, (float) w);
    setParam (freq, (float) std::sqrt (lo * hi));
}

void SpectrumDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (zoomButton.contains (e.position))
    {
        zoomed = ! zoomed;
        if (zoomed)
        {
            viewSpanOct = zoomSpanOct;
            viewLoOct = std::log2 (freq.convertFrom0to1 (freq.getValue())) - 0.5 * zoomSpanOct;
        }
        else
        {
            viewSpanOct = fullHiOct - fullLoOct;
            viewLoOct = fullLoOct;
        }
        clampView();
        repaint();
        return;
    }

    drag = hitTestDrag (e.position);
    dragStartY = e.position.y;
    dragStartViewLo = viewLoOct;

    double lo, hi;
    bandEdges (lo, hi);

    switch (drag)
    {
        case Drag::low:
        case Drag::high:
            dragOtherEdge = drag == Drag::low ? hi : lo;
            freq.beginChangeGesture();
            width.beginChangeGesture();
            break;
        case Drag::centre:
            dragStartValue = (float) std::log2 (freq.convertFrom0to1 (freq.getValue()));
            freq.beginChangeGesture();
            break;
        case Drag::threshold:
            dragStartValue = threshold.convertFrom0to1 (threshold.getValue());
            threshold.beginChangeGesture();
            break;
        case Drag::scroll:
        case Drag::none:
            break;
    }
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const auto offset = e.getOffsetFromDragStart().toFloat();

    switch (drag)
    {
        case Drag::low:
            setBandFromEdges (std::min (freqForX (e.position.x), dragOtherEdge * 0.93), dragOtherEdge);
            break;
        case Drag::high:
            setBandFromEdges (dragOtherEdge, std::max (freqForX (e.position.x), dragOtherEdge * 1.07));
            break;
        case Drag::centre:
            setParam (freq, (float) std::exp2 (dragStartValue + offset.x / plot.getWidth() * viewSpanOct));
            break;
        case Drag::threshold:
            setParam (threshold, dragStartValue + (dbForY (e.position.y) - dbForY (dragStartY)));
            break;
        case Drag::scroll:
            viewLoOct = dragStartViewLo + offset.x / scrollTrack.getWidth() * (fullHiOct - fullLoOct);
            clampView();
            break;
        case Drag::none:
            break;
    }

    repaint();
}

void SpectrumDisplay::mouseUp (const juce::MouseEvent&)
{
    switch (drag)
    {
        case Drag::low:
        case Drag::high:
            freq.endChangeGesture();
            width.endChangeGesture();
            break;
        case Drag::centre:    freq.endChangeGesture(); break;
        case Drag::threshold: threshold.endChangeGesture(); break;
        case Drag::scroll:
        case Drag::none:      break;
    }

    drag = Drag::none;
}

void SpectrumDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! plot.contains (e.position))
        return;

    width.beginChangeGesture();
    setParam (width, width.convertFrom0to1 (width.getValue()) + wheel.deltaY * 0.5f);
    width.endChangeGesture();
    repaint();
}

} // namespace rde::ui
