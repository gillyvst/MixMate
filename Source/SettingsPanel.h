#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace mixmate
{

// Full-window overlay: crossover frequencies, shortcut keys, size and defaults.
class SettingsPanel : public juce::Component
{
public:
    explicit SettingsPanel (MixMateProcessor& p);

    void open();
    void close();

    // While waiting for a new shortcut key, the editor hands key presses here.
    bool isLearningKey() const                  { return learning >= 0; }
    void learnKey (const juce::KeyPress& key);

    std::function<void()> onClose;
    std::function<void (float)> onScaleChanged;
    std::function<void()> onKeysChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refresh();

    MixMateProcessor& processor;
    int learning = -1;

    juce::Label title, xoverHeading, keysHeading, sizeHeading, credits;
    juce::Label xoverLabels[3], keyLabels[3];
    juce::Slider xoverSliders[3];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xoverAttachments[3];
    juce::TextButton keyButtons[3];
    juce::ToggleButton levelMatchToggle { "Level match (equal loudness)" };
    juce::ToggleButton exportToggle { "Bypass when exporting" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> levelMatchAttachment;
    juce::TextButton sizeButtons[3];
    juce::TextButton saveDefaultButton { "Save as default" };
    juce::TextButton resetButton { "Reset" };
    juce::TextButton doneButton { "Done" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};

} // namespace mixmate
