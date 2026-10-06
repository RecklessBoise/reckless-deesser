#include "PluginEditor.h"

namespace
{
using namespace rde;

/** The layout is transcribed from a 590 x 335 reference: convert those
    coordinates (x1, y1, x2, y2) to the 1.5x logical canvas. */
juce::Rectangle<int> R (float x1, float y1, float x2, float y2)
{
    constexpr float s = 1.5f, ox = 5.0f, oy = 133.0f;
    return juce::Rectangle<float> ((x1 - ox) * s, (y1 - oy) * s, (x2 - x1) * s, (y2 - y1) * s).toNearestInt();
}

struct Label
{
    const char* text;
    float x1, y1, x2, y2, size;
    bool bold;
};

constexpr Label mainLabels[] = {
    { "INPUT", 5, 152, 55, 165, 13.0f, true },        { "LISTEN", 55, 152, 111, 165, 13.0f, true },
    { "FILTERS", 55, 286, 111, 299, 13.0f, true },    { "DYNAMICS", 443, 152, 545, 165, 13.0f, true },
    { "OUTPUT", 545, 152, 595, 165, 13.0f, true },    { "TRIM", 5, 387, 55, 397, 10.5f, false },
    { "MIX", 55, 170, 111, 180, 10.5f, false },       { "INSIDE", 55, 208, 111, 218, 10.5f, false },
    { "OUTSIDE", 55, 245, 111, 255, 10.5f, false },   { "WIDTH", 55, 311, 111, 321, 10.5f, false },
    { "SLOPE/Q", 55, 349, 111, 359, 10.5f, false },   { "FREQUENCY", 55, 387, 111, 397, 10.5f, false },
    { "REDUCTION", 443, 171, 496, 181, 10.5f, false }, { "RATIO", 494, 171, 545, 181, 10.5f, false },
    { "MAKE-UP", 443, 356, 494, 366, 10.5f, false },  { "WET/DRY", 443, 387, 494, 397, 10.5f, false },
    { "SOFT-KNEE", 494, 387, 545, 397, 10.5f, false }, { "TRIM", 545, 356, 595, 366, 10.5f, false },
    { "EFFECT", 545, 380, 595, 390, 10.5f, false },   { "LIGHT\nCOMP", 519, 216, 545, 236, 8.5f, false },
    { "HEAVY\nCOMP", 519, 280, 545, 300, 8.5f, false }, { "CUT", 519, 343, 545, 353, 8.5f, false },
};

constexpr Label advancedLabels[] = {
    { "EFFECT\nMODE:", 10, 437, 60, 465, 12.5f, true },        { "TRIGGER", 58, 432, 103, 442, 10.5f, false },
    { "AUDIO", 103, 432, 145, 442, 10.5f, false },             { "REACTION\nENVELOPE:", 155, 437, 220, 465, 12.5f, true },
    { "ATTACK", 220, 437, 268, 447, 10.5f, false },            { "HOLD", 278, 437, 325, 447, 10.5f, false },
    { "RELEASE", 340, 437, 389, 447, 10.5f, false },           { "LEVEL\nTRACKING:", 405, 437, 472, 465, 12.5f, true },
    { "AUTO", 474, 432, 518, 442, 10.5f, false },              { "DAMPING", 520, 437, 572, 447, 10.5f, false },
};
} // namespace

//==============================================================================
MainPanel::MainPanel (RecklessDeEsserProcessor& p) : processor (p), display (p)
{
    setLookAndFeel (&lnf);

    for (auto* c : std::initializer_list<juce::Component*> {
             &inputMeter, &outputMeter, &reductionMeter, &inTrim, &outTrim, &width, &slope, &freq, &makeup,
             &ratioBox, &mix, &knee, &attack, &hold, &release, &damping, &thresholdFader, &ratioFader,
             &mixButton, &insideButton, &outsideButton, &effectButton, &autoButton, &triggerButton,
             &audioButton, &sizeButton, &advancedButton, &display })
        addAndMakeVisible (c);

    attach (inTrim, ids::inTrim);
    attach (outTrim, ids::outTrim);
    attach (width, ids::width);
    attach (slope, ids::slope);
    attach (freq, ids::freq);
    attach (makeup, ids::makeup);
    attach (ratioBox, ids::ratio);
    attach (ratioFader, ids::ratio);
    attach (mix, ids::mix);
    attach (knee, ids::knee);
    attach (attack, ids::attack);
    attach (hold, ids::hold);
    attach (release, ids::release);
    attach (damping, ids::damping);
    attach (thresholdFader, ids::threshold);

    const auto inOut = [] (bool on) { return juce::String (on ? "IN" : "OUT"); };
    const auto bandWide = [] (bool on) { return juce::String (on ? "WIDE" : "BAND"); };
    effectButton.textForState = inOut;
    autoButton.textForState = inOut;
    triggerButton.textForState = bandWide;
    audioButton.textForState = bandWide;

    for (auto* b : { &effectButton, &autoButton })
        b->setClickingTogglesState (true);

    effectAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ids::effect, effectButton);
    autoAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ids::autoTrack, autoButton);

    auto& state = processor.apvts;
    listenButtons = std::make_unique<ui::ChoiceButtons> (*state.getParameter (ids::listen),
                                                         std::vector<juce::Button*> { &mixButton, &insideButton, &outsideButton });
    triggerChoice = std::make_unique<ui::ChoiceButtons> (*state.getParameter (ids::trigger), std::vector<juce::Button*> { &triggerButton }, true);
    audioChoice = std::make_unique<ui::ChoiceButtons> (*state.getParameter (ids::audio), std::vector<juce::Button*> { &audioButton }, true);

    for (auto* b : { &sizeButton, &advancedButton })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        b->setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    }

    sizeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff1a2040));
    sizeButton.setTooltip ("Plug-in size");
    sizeButton.onClick = [this] { if (onSizeMenu) onSizeMenu(); };

    advancedButton.setColour (juce::TextButton::textColourOffId, rde::theme::label);
    advancedButton.onClick = [this] { if (onToggleAdvanced) onToggleAdvanced(); };

    setAdvancedOpen (processor.advancedOpen.load());
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MainPanel::attach (juce::Slider& s, const char* id)
{
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processor.apvts, id, s));

    if (auto* param = processor.apvts.getParameter (id))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void MainPanel::setAdvancedOpen (bool open)
{
    advancedOpen = open;
    advancedButton.setButtonText (open ? juce::String::fromUTF8 ("ADVANCED   \xe2\x96\xb2   CONTROLS")
                                       : juce::String::fromUTF8 ("ADVANCED   \xe2\x96\xbc   CONTROLS"));

    for (auto* c : std::initializer_list<juce::Component*> { &triggerButton, &audioButton, &attack, &hold, &release, &autoButton, &damping })
        c->setVisible (open);

    repaint();
}

void MainPanel::resized()
{
    sizeButton.setBounds (R (548, 134, 595, 149));

    inputMeter.setBounds (R (19, 172, 52, 351));
    inTrim.setBounds (R (8, 399, 52, 411));

    mixButton.setBounds (R (66, 181, 96, 202));
    insideButton.setBounds (R (66, 219, 96, 240));
    outsideButton.setBounds (R (66, 256, 96, 277));
    width.setBounds (R (62, 322, 104, 334));
    slope.setBounds (R (62, 360, 104, 372));
    freq.setBounds (R (62, 399, 104, 411));

    display.setBounds (R (113, 152, 420, 413));
    thresholdFader.setBounds (R (421, 152, 441, 398));

    reductionMeter.setBounds (R (461, 181, 494, 352));
    ratioFader.setBounds (R (499, 183, 519, 357));
    makeup.setBounds (R (448, 367, 490, 379));
    ratioBox.setBounds (R (497, 367, 540, 379));
    mix.setBounds (R (448, 399, 490, 411));
    knee.setBounds (R (497, 399, 540, 411));

    outputMeter.setBounds (R (561, 172, 594, 351));
    outTrim.setBounds (R (549, 367, 591, 379));
    effectButton.setBounds (R (556, 392, 584, 410));

    advancedButton.setBounds (R (200, 415, 400, 428));

    triggerButton.setBounds (R (65, 444, 96, 462));
    audioButton.setBounds (R (109, 444, 139, 462));
    attack.setBounds (R (222, 449, 266, 461));
    hold.setBounds (R (281, 449, 322, 461));
    release.setBounds (R (343, 449, 386, 461));
    autoButton.setBounds (R (482, 444, 511, 462));
    damping.setBounds (R (524, 449, 568, 461));
}

void MainPanel::drawSection (juce::Graphics& g, juce::Rectangle<int> r) const
{
    const auto f = r.toFloat().reduced (1.0f);
    juce::ColourGradient grad (rde::theme::panelTop, f.getX(), f.getY(), rde::theme::panelBottom, f.getX(), f.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawHorizontalLine ((int) f.getY() + 1, f.getX() + 1.0f, f.getRight() - 1.0f);
    g.drawVerticalLine ((int) f.getX() + 1, f.getY() + 1.0f, f.getBottom() - 1.0f);
    g.setColour (rde::theme::panelBorder);
    g.drawRect (f, 1.5f);
}

void MainPanel::drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, float size, bool bold) const
{
    g.setFont (rde::theme::font (size, bold));
    const int lines = text.containsChar ('\n') ? 2 : 1;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawFittedText (text, r.translated (1, 1), juce::Justification::centred, lines, 0.7f);
    g.setColour (rde::theme::label);
    g.drawFittedText (text, r, juce::Justification::centred, lines, 0.7f);
}

void MainPanel::paint (juce::Graphics& g)
{
    using namespace rde;
    g.fillAll (theme::background);

    // header
    const auto header = R (5, 133, 595, 150);
    juce::ColourGradient hg (theme::header.brighter (0.25f), 0.0f, (float) header.getY(), theme::header.darker (0.2f), 0.0f, (float) header.getBottom(), false);
    g.setGradientFill (hg);
    g.fillRect (header);

    for (auto box : { R (6, 134, 70, 149), R (548, 134, 594, 149) })
    {
        g.setColour (theme::logoBox);
        g.fillRect (box);
        g.setColour (theme::panelBorder);
        g.drawRect (box, 1);
    }

    g.setColour (juce::Colour (0xff111426));
    g.setFont (theme::font (14.0f, true));
    g.drawText ("Reckless", R (6, 134, 70, 149), juce::Justification::centred);

    g.setColour (theme::headerText);
    g.setFont (theme::font (16.0f));
    g.drawText ("RECKLESS DEESSER", R (70, 133, 548, 150), juce::Justification::centred);

    // main sections
    for (auto r : { R (5, 150, 55, 415), R (55, 150, 111, 415), R (443, 150, 545, 415), R (545, 150, 595, 415) })
        drawSection (g, r);

    g.setColour (theme::displayBg);
    g.fillRect (R (111, 150, 443, 415));
    g.setColour (theme::panelBorder);
    g.drawRect (R (111, 150, 443, 415), 2);

    g.setColour (theme::panelBorder);
    g.drawHorizontalLine (R (55, 283, 111, 284).getY(), (float) R (56, 0, 0, 0).getX(), (float) R (110, 0, 0, 0).getX());

    for (const auto& l : mainLabels)
        drawLabel (g, l.text, R (l.x1, l.y1, l.x2, l.y2), l.size, l.bold);

    // advanced strip
    const auto strip = R (5, 415, 595, 428);
    g.setColour (theme::strip);
    g.fillRect (strip);
    g.setColour (theme::panelBorder);
    g.drawRect (strip, 1);

    if (advancedOpen)
    {
        for (auto r : { R (5, 428, 150, 468), R (150, 428, 398, 468), R (398, 428, 595, 468) })
            drawSection (g, r);

        for (const auto& l : advancedLabels)
            drawLabel (g, l.text, R (l.x1, l.y1, l.x2, l.y2), l.size, l.bold);
    }
}

void MainPanel::timerCallback()
{
    const float inPk = processor.inputPeak.exchange (0.0f);
    const float outPk = processor.outputPeak.exchange (0.0f);
    const float gr = processor.gainReductionDb.exchange (0.0f);

    inputMeter.setValueDb (juce::Decibels::gainToDecibels (inPk, -100.0f));
    outputMeter.setValueDb (juce::Decibels::gainToDecibels (outPk, -100.0f));
    reductionMeter.setValueDb (gr);
    display.refresh (gr, processor.effectiveThresholdDb.load());
}

//==============================================================================
RecklessDeEsserEditor::RecklessDeEsserEditor (RecklessDeEsserProcessor& p)
    : AudioProcessorEditor (p), deEsser (p), panel (p)
{
    addAndMakeVisible (panel);

    panel.onToggleAdvanced = [this] {
        const float scale = (float) getWidth() / (float) MainPanel::kWidth;
        deEsser.advancedOpen = ! deEsser.advancedOpen.load();
        panel.setAdvancedOpen (deEsser.advancedOpen.load());
        updateConstraints();
        setScale (scale);
    };

    panel.onSizeMenu = [this] { showSizeMenu(); };

    // Read before setResizeLimits(), which resizes (and so overwrites uiScale) from the initial 0x0 bounds.
    const float savedScale = deEsser.uiScale.load();
    setResizable (true, true);
    updateConstraints();
    setScale (savedScale);
}

int RecklessDeEsserEditor::logicalHeight() const
{
    return deEsser.advancedOpen.load() ? MainPanel::kHeightFull : MainPanel::kHeightCompact;
}

void RecklessDeEsserEditor::updateConstraints()
{
    const int h = logicalHeight();
    setResizeLimits (MainPanel::kWidth / 2, h / 2, (MainPanel::kWidth * 5) / 2, (h * 5) / 2);

    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) MainPanel::kWidth / (double) h);
}

void RecklessDeEsserEditor::setScale (float scale)
{
    scale = juce::jlimit (0.5f, 2.5f, scale);
    setSize (juce::roundToInt (MainPanel::kWidth * scale), juce::roundToInt ((float) logicalHeight() * scale));
}

void RecklessDeEsserEditor::showSizeMenu()
{
    juce::PopupMenu menu;
    const float current = deEsser.uiScale.load();
    constexpr int percents[] = { 50, 75, 100, 125, 150, 175, 200, 250 };

    for (int pct : percents)
        menu.addItem (pct, juce::String (pct) + " %", true, std::abs (current * 100.0f - (float) pct) < 1.0f);

    menu.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this), [safe = juce::Component::SafePointer (this)] (int result) {
        if (safe != nullptr && result > 0)
            safe->setScale ((float) result / 100.0f);
    });
}

void RecklessDeEsserEditor::paint (juce::Graphics& g)
{
    g.fillAll (rde::theme::background);
}

void RecklessDeEsserEditor::resized()
{
    if (getWidth() <= 0)
        return;

    const float scale = (float) getWidth() / (float) MainPanel::kWidth;
    panel.setBounds (0, 0, MainPanel::kWidth, logicalHeight());
    panel.setTransform (juce::AffineTransform::scale (scale));
    deEsser.uiScale = scale;
    panel.setScaleText (juce::String (juce::roundToInt (scale * 100.0f)) + "%");
}
