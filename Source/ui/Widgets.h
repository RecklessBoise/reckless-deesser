#pragma once

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace rde::ui
{

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    int getSliderThumbRadius (juce::Slider&) override;
    juce::Font getPopupMenuFont() override { return theme::font (14.0f); }
};

/** Dark numeric read-out that is edited by dragging vertically (double-click resets). */
class ValueBox : public juce::Slider
{
public:
    ValueBox();
    void paint (juce::Graphics&) override;
};

/** Vertical fader with a silver cap, optionally inverted (max at the bottom). */
class Fader : public juce::Slider
{
public:
    Fader (int capHalfHeight, bool inverted);
    double proportionOfLengthToValue (double proportion) override;
    double valueToProportionOfLength (double value) override;
    void paint (juce::Graphics&) override;

    const int capHalfHeight;

private:
    const bool inverted;
};

/** Square hardware-style push button with an optional icon or state-dependent text. */
class LedButton : public juce::Button
{
public:
    enum class Icon { none, mix, bandPass, notch };

    LedButton (Icon icon, juce::Colour onColour);
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;

    std::function<juce::String (bool on)> textForState;

private:
    void drawIcon (juce::Graphics&, juce::Rectangle<float>, juce::Colour) const;

    Icon icon;
    juce::Colour onColour;
};

/** Segmented LED meter with its scale drawn to the right of the bar. */
class LevelMeter : public juce::Component
{
public:
    enum class Kind { level, reduction };

    LevelMeter (Kind kind, int barWidth);
    void setValueDb (float db); // level: peak dBFS; reduction: gain change (<= 0)
    void paint (juce::Graphics&) override;

private:
    float proportionFor (float db) const; // 0 = bottom (level) / top (reduction), 1 = other end

    Kind kind;
    int barWidth;
    float shownDb;
};

/** Ties a group of buttons to the indices of a choice/bool parameter. */
class ChoiceButtons
{
public:
    ChoiceButtons (juce::RangedAudioParameter& param, std::vector<juce::Button*> buttons, bool toggleSingle = false);

private:
    void update (float value);

    std::vector<juce::Button*> buttons;
    juce::ParameterAttachment attachment;
    bool toggleSingle;
    int current = 0;
};

} // namespace rde::ui
