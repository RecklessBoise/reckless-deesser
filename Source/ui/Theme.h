#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace rde::theme
{
inline const juce::Colour panelTop { 0xff7387be };
inline const juce::Colour panelBottom { 0xff566aa3 };
inline const juce::Colour panelBorder { 0xff26335c };
inline const juce::Colour background { 0xff4a5c92 };
inline const juce::Colour header { 0xff33467c };
inline const juce::Colour headerText { 0xffd6defc };
inline const juce::Colour logoBox { 0xffdfe3ec };
inline const juce::Colour strip { 0xff3b4d85 };

inline const juce::Colour displayBg { 0xff15224a };
inline const juce::Colour displayBg2 { 0xff1e2d5a };
inline const juce::Colour grid { 0xff2e4777 };
inline const juce::Colour gridText { 0xffbccaee };

inline const juce::Colour valueBg { 0xff0b1330 };
inline const juce::Colour valueText { 0xffcfe2ff };
inline const juce::Colour label { 0xfff2f5ff };

inline const juce::Colour cyan { 0xff86e6ff };
inline const juce::Colour yellow { 0xfff3de4f };
inline const juce::Colour red { 0xffe0365a };
inline const juce::Colour spectrumIn { 0xffc4378c };
inline const juce::Colour spectrumOut { 0xff3d52cc };

inline const juce::Colour ledGreen { 0xff83e683 };
inline const juce::Colour ledGreenOff { 0xff27412d };
inline const juce::Colour ledRed { 0xffff5b4f };
inline const juce::Colour grYellow { 0xfff6d343 };
inline const juce::Colour grOrange { 0xffef7b25 };
inline const juce::Colour grOff { 0xff3c3420 };

inline const juce::Colour buttonOnTeal { 0xff69e6b8 };
inline const juce::Colour buttonOnGreen { 0xff78de6c };
inline const juce::Colour buttonOnBlue { 0xff9fd8ff };

inline juce::Font font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace rde::theme
