// Renders the editor off-screen to PNG files (used for docs and visual checks).
// Usage: RecklessDeEsserSnapshot <output-dir>

#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <cmath>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                       : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    RecklessDeEsserProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    // Feed a vocal-like test signal (harmonics + sibilant noise bursts) so the display has content.
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (7);
    double phase = 0.0;
    int sample = 0;

    // Sibilant noise bursts are on for the first third of every 6000-sample cycle.
    auto feed = [&] (int numBlocks) {
        for (int block = 0; block < numBlocks; ++block)
        {
            for (int i = 0; i < 512; ++i, ++sample)
            {
                float s = 0.0f;

                for (int h = 1; h <= 12; ++h)
                    s += 0.12f / (float) h * (float) std::sin (phase * h);

                const bool ess = (sample / 2000) % 3 == 0;
                s += (ess ? 0.35f : 0.01f) * (rng.nextFloat() * 2.0f - 1.0f);
                phase += juce::MathConstants<double>::twoPi * 180.0 / 48000.0;
                buffer.setSample (0, i, s);
                buffer.setSample (1, i, s);
            }

            processor.processBlock (buffer, midi);
        }
    };

    feed (200);

    // The editor must reopen at the size saved in the session.
    processor.uiScale = 1.25f;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());

    if (editor->getWidth() != juce::roundToInt (885.0f * 1.25f))
    {
        std::printf ("editor opened at %d px wide, expected %d\n", editor->getWidth(), juce::roundToInt (885.0f * 1.25f));
        return 1;
    }

    for (float scale : { 1.0f, 1.5f })
    {
        editor->setSize (juce::roundToInt (885.0f * scale), juce::roundToInt (503.0f * scale));

        // let the editor timer pull the analyser data
        for (int i = 0; i < 6; ++i)
        {
            feed (3);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const auto file = outDir.getChildFile ("ui-" + juce::String (juce::roundToInt (scale * 100.0f)) + ".png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
        std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    }

    processor.editorBeingDeleted (editor.get());
    editor.reset();
    return 0;
}
