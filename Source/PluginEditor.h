#pragma once

#include "PluginProcessor.h"
#include "ui/SpectrumDisplay.h"
#include "ui/Widgets.h"

/** All controls, laid out on a fixed logical canvas; the editor scales it as a whole. */
class MainPanel final : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 885;
    static constexpr int kHeightFull = 503;
    static constexpr int kHeightCompact = 443;

    explicit MainPanel (RecklessDeEsserProcessor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void setAdvancedOpen (bool open);
    void setScaleText (const juce::String& text) { sizeButton.setButtonText (text); }

    std::function<void()> onToggleAdvanced, onSizeMenu;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void attach (juce::Slider&, const char* id);
    void drawSection (juce::Graphics&, juce::Rectangle<int>) const;
    void drawLabel (juce::Graphics&, const juce::String&, juce::Rectangle<int>, float size, bool bold = false) const;

    RecklessDeEsserProcessor& processor;
    rde::ui::LookAndFeel lnf;

    rde::ui::LevelMeter inputMeter { rde::ui::LevelMeter::Kind::level, 15 };
    rde::ui::LevelMeter outputMeter { rde::ui::LevelMeter::Kind::level, 15 };
    rde::ui::LevelMeter reductionMeter { rde::ui::LevelMeter::Kind::reduction, 15 };

    rde::ui::ValueBox inTrim, outTrim, width, slope, freq, makeup, ratioBox, mix, knee, attack, hold, release, damping;
    rde::ui::Fader thresholdFader { 24, false }, ratioFader { 22, true };

    rde::ui::LedButton mixButton { rde::ui::LedButton::Icon::mix, rde::theme::buttonOnTeal };
    rde::ui::LedButton insideButton { rde::ui::LedButton::Icon::bandPass, rde::theme::buttonOnTeal };
    rde::ui::LedButton outsideButton { rde::ui::LedButton::Icon::notch, rde::theme::buttonOnTeal };
    rde::ui::LedButton effectButton { rde::ui::LedButton::Icon::none, rde::theme::buttonOnGreen };
    rde::ui::LedButton autoButton { rde::ui::LedButton::Icon::none, rde::theme::buttonOnGreen };
    rde::ui::LedButton triggerButton { rde::ui::LedButton::Icon::none, rde::theme::buttonOnBlue };
    rde::ui::LedButton audioButton { rde::ui::LedButton::Icon::none, rde::theme::buttonOnBlue };

    juce::TextButton sizeButton { "100%" };
    juce::TextButton advancedButton;

    rde::ui::SpectrumDisplay display;

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ButtonAttachment> effectAttachment, autoAttachment;
    std::unique_ptr<rde::ui::ChoiceButtons> listenButtons, triggerChoice, audioChoice;

    bool advancedOpen = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

class RecklessDeEsserEditor final : public juce::AudioProcessorEditor
{
public:
    explicit RecklessDeEsserEditor (RecklessDeEsserProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int logicalHeight() const;
    void updateConstraints();
    void setScale (float scale);
    void showSizeMenu();

    RecklessDeEsserProcessor& deEsser;
    MainPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecklessDeEsserEditor)
};
