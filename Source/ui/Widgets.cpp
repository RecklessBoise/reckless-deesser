#include "Widgets.h"

namespace rde::ui
{

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, theme::displayBg2);
    setColour (juce::PopupMenu::textColourId, theme::label);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::panelTop);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

int LookAndFeel::getSliderThumbRadius (juce::Slider& s)
{
    if (auto* f = dynamic_cast<Fader*> (&s))
        return f->capHalfHeight;

    return LookAndFeel_V4::getSliderThumbRadius (s);
}

//==============================================================================
ValueBox::ValueBox() : juce::Slider (RotaryVerticalDrag, NoTextBox)
{
    setMouseDragSensitivity (220);
    setVelocityBasedMode (false);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void ValueBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::valueBg);
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine ((int) r.getBottom(), r.getX() + 2.0f, r.getRight() - 2.0f);

    g.setColour (isMouseOverOrDragging() ? juce::Colours::white : theme::valueText);
    g.setFont (theme::font (r.getHeight() * 0.62f));
    g.drawFittedText (getTextFromValue (getValue()), getLocalBounds().reduced (2, 0), juce::Justification::centred, 1, 0.8f);
}

//==============================================================================
Fader::Fader (int halfHeight, bool invert)
    : juce::Slider (LinearVertical, NoTextBox), capHalfHeight (halfHeight), inverted (invert)
{
    setSliderSnapsToMousePosition (false);
}

double Fader::proportionOfLengthToValue (double proportion)
{
    return juce::Slider::proportionOfLengthToValue (inverted ? 1.0 - proportion : proportion);
}

double Fader::valueToProportionOfLength (double value)
{
    const double p = juce::Slider::valueToProportionOfLength (value);
    return inverted ? 1.0 - p : p;
}

void Fader::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX();
    const float top = (float) capHalfHeight, bottom = b.getHeight() - (float) capHalfHeight;

    // slot
    auto slot = juce::Rectangle<float> (cx - 2.5f, top - 4.0f, 5.0f, bottom - top + 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (slot, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawVerticalLine ((int) slot.getRight(), slot.getY() + 1.0f, slot.getBottom() - 1.0f);

    // cap
    const float y = (float) getPositionOfValue (getValue());
    auto cap = juce::Rectangle<float> (b.getX() + 1.0f, y - (float) capHalfHeight, b.getWidth() - 2.0f, 2.0f * (float) capHalfHeight);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (cap.translated (1.5f, 2.0f), 2.5f);

    juce::ColourGradient grad (juce::Colour (0xfff2f3f7), cap.getX(), cap.getY(),
                               juce::Colour (0xff8c93a6), cap.getX(), cap.getBottom(), false);
    grad.addColour (0.45, juce::Colour (0xffd0d4de));
    grad.addColour (0.55, juce::Colour (0xffa9afbf));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (juce::Colour (0xff3a4058));
    g.drawRoundedRectangle (cap, 2.5f, 1.0f);

    // grip lines
    for (int i = -3; i <= 3; ++i)
    {
        const float ly = cap.getCentreY() + (float) i * cap.getHeight() * 0.1f;
        g.setColour (juce::Colours::black.withAlpha (i == 0 ? 0.75f : 0.35f));
        g.drawHorizontalLine ((int) ly, cap.getX() + 3.0f, cap.getRight() - 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawHorizontalLine ((int) ly + 1, cap.getX() + 3.0f, cap.getRight() - 3.0f);
    }
}

//==============================================================================
LedButton::LedButton (Icon i, juce::Colour on) : juce::Button ({}), icon (i), onColour (on)
{
}

void LedButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();

    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (1.0f, 1.5f), 2.5f);

    const auto base = on ? onColour : juce::Colour (0xffc9cdd8);
    juce::ColourGradient grad (base.brighter (0.35f), r.getX(), r.getY(),
                               base.darker (on ? 0.25f : 0.45f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.5f);

    if (on)
    {
        g.setColour (onColour.withAlpha (0.35f));
        g.drawRoundedRectangle (r.expanded (1.0f), 3.5f, 2.0f);
    }

    if (isHighlighted || isDown)
    {
        g.setColour (juce::Colours::white.withAlpha (isDown ? 0.25f : 0.12f));
        g.fillRoundedRectangle (r, 2.5f);
    }

    g.setColour (juce::Colour (0xff2b3250));
    g.drawRoundedRectangle (r, 2.5f, 1.0f);

    const auto ink = juce::Colour (0xff1a2040);

    if (textForState)
    {
        g.setColour (ink);
        g.setFont (theme::font (r.getHeight() * 0.42f, true));
        g.drawFittedText (textForState (on), r.toNearestInt(), juce::Justification::centred, 1);
    }
    else
    {
        drawIcon (g, r.reduced (r.getWidth() * 0.2f, r.getHeight() * 0.3f), ink);
    }
}

void LedButton::drawIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) const
{
    g.setColour (c);
    juce::Path p;
    const float x0 = r.getX(), x1 = r.getRight(), y0 = r.getY(), y1 = r.getBottom();
    const float w = r.getWidth();

    switch (icon)
    {
        case Icon::mix:
        {
            for (float cx : { x0 + w * 0.27f, x0 + w * 0.73f })
            {
                p.startNewSubPath (cx, y0);
                p.lineTo (cx, y1);
                p.startNewSubPath (cx - w * 0.2f, r.getCentreY());
                p.lineTo (cx + w * 0.2f, r.getCentreY());
            }
            break;
        }
        case Icon::bandPass:
            p.startNewSubPath (x0, y1);
            p.lineTo (x0 + w * 0.25f, y1);
            p.lineTo (x0 + w * 0.25f, y0);
            p.lineTo (x0 + w * 0.75f, y0);
            p.lineTo (x0 + w * 0.75f, y1);
            p.lineTo (x1, y1);
            break;
        case Icon::notch:
            p.startNewSubPath (x0, y0);
            p.lineTo (x0 + w * 0.25f, y0);
            p.lineTo (x0 + w * 0.25f, y1);
            p.lineTo (x0 + w * 0.75f, y1);
            p.lineTo (x0 + w * 0.75f, y0);
            p.lineTo (x1, y0);
            break;
        case Icon::none:
            return;
    }

    g.strokePath (p, juce::PathStrokeType (1.8f));
}

//==============================================================================
namespace
{
    constexpr float levelTicks[] = { 0.0f, -2.0f, -4.0f, -6.0f, -8.0f, -12.0f, -16.0f, -20.0f, -24.0f, -28.0f, -32.0f, -40.0f };
    constexpr int numLevelTicks = (int) std::size (levelTicks);
} // namespace

LevelMeter::LevelMeter (Kind k, int bw) : kind (k), barWidth (bw), shownDb (k == Kind::level ? -100.0f : 0.0f)
{
    setInterceptsMouseClicks (false, false);
}

void LevelMeter::setValueDb (float db)
{
    if (kind == Kind::level)
        shownDb = db > shownDb ? db : std::max (db, shownDb - 1.5f);
    else
        shownDb = db < shownDb ? db : std::min (db, shownDb + 1.0f);

    repaint();
}

float LevelMeter::proportionFor (float db) const
{
    if (kind == Kind::reduction)
        return juce::jlimit (0.0f, 1.0f, -db / 24.0f);

    if (db >= levelTicks[0])
        return 1.0f;

    for (int i = 1; i < numLevelTicks; ++i)
    {
        if (db >= levelTicks[i])
        {
            const float frac = (db - levelTicks[i]) / (levelTicks[i - 1] - levelTicks[i]);
            return ((float) (numLevelTicks - 1 - i) + frac) / (float) (numLevelTicks - 1);
        }
    }

    return 0.0f;
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float pad = 6.0f;
    auto bar = juce::Rectangle<float> (b.getX(), b.getY() + pad, (float) barWidth, b.getHeight() - 2.0f * pad);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRect (bar.expanded (1.5f));

    const int segments = kind == Kind::level ? 44 : 32;
    const float segH = bar.getHeight() / (float) segments;
    const float lit = proportionFor (shownDb);

    for (int s = 0; s < segments; ++s)
    {
        juce::Colour on, off;
        float segProp;
        juce::Rectangle<float> seg;

        if (kind == Kind::level)
        {
            segProp = (float) (s + 1) / (float) segments; // from the bottom
            seg = { bar.getX(), bar.getBottom() - (float) (s + 1) * segH, bar.getWidth(), segH };
            on = segProp > 0.93f ? theme::ledRed : theme::ledGreen;
            off = segProp > 0.93f ? theme::ledRed.darker (2.0f) : theme::ledGreenOff;
        }
        else
        {
            segProp = (float) (s + 1) / (float) segments; // from the top
            seg = { bar.getX(), bar.getY() + (float) s * segH, bar.getWidth(), segH };
            on = theme::grYellow.interpolatedWith (theme::grOrange, segProp);
            off = theme::grOff;
        }

        const bool isLit = segProp - 0.5f / (float) segments <= lit && lit > 0.0f;
        g.setColour (isLit ? on : off);
        g.fillRect (seg.reduced (0.5f, 0.6f));
    }

    // scale
    g.setFont (theme::font (9.0f));
    g.setColour (theme::label.withAlpha (0.9f));
    const float tx = bar.getRight() + 4.0f;

    auto drawTick = [&] (float prop, const juce::String& text, bool fromTop) {
        const float y = fromTop ? bar.getY() + prop * bar.getHeight() : bar.getBottom() - prop * bar.getHeight();
        g.drawText (text, juce::Rectangle<float> (tx, y - 5.0f, b.getRight() - tx, 10.0f), juce::Justification::centredLeft, false);
    };

    if (kind == Kind::level)
    {
        for (float t : levelTicks)
            drawTick (proportionFor (t), juce::String ((int) t), false);
    }
    else
    {
        for (int t = 3; t <= 24; t += 3)
            drawTick ((float) t / 24.0f, juce::String (t), true);
    }
}

//==============================================================================
ChoiceButtons::ChoiceButtons (juce::RangedAudioParameter& param, std::vector<juce::Button*> b, bool single)
    : buttons (std::move (b)),
      attachment (param, [this] (float v) { update (v); }, nullptr),
      toggleSingle (single)
{
    for (size_t i = 0; i < buttons.size(); ++i)
    {
        buttons[i]->onClick = [this, i] {
            const int target = toggleSingle ? (current == 0 ? 1 : 0) : (int) i;
            attachment.setValueAsCompleteGesture ((float) target);
        };
    }

    attachment.sendInitialUpdate();
}

void ChoiceButtons::update (float value)
{
    current = juce::roundToInt (value);

    for (size_t i = 0; i < buttons.size(); ++i)
    {
        const bool on = toggleSingle ? current == 1 : current == (int) i;
        buttons[i]->setToggleState (on, juce::dontSendNotification);
    }
}

} // namespace rde::ui
