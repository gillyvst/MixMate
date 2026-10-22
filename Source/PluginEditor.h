#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Theme.h"
#include "HeadphonePicker.h"
#include "SettingsPanel.h"

namespace mixmate
{

// A row of buttons where one option is chosen.
class SegmentedControl : public juce::Component
{
public:
    explicit SegmentedControl (juce::StringArray labels);

    void setSelected (int index);
    int getSelected() const                     { return selected; }
    void setSegmentEnabled (int index, bool enabled);
    void setHighlight (int index, juce::Colour fill, juce::Colour light);

    std::function<void (int)> onClick;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int segmentAt (juce::Point<float> p) const;

    juce::StringArray labels;
    std::vector<bool> enabled;
    std::vector<juce::Colour> fills;
    int selected = -1;
    int hovered = -1;
};

// Small graph of a headphone's correction curve.
class CurveView : public juce::Component
{
public:
    void setCurve (const EqCurve& curve, float strength);
    void setActive (bool shouldBeActive);

    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    std::vector<float> responseDb;
    bool active = false;
};

// One side of the plugin: the curve, the headphone name and where the data came from.
class HeadphonePanel : public juce::Component
{
public:
    explicit HeadphonePanel (int slot);

    void update (const Headphone& hp, float strength, bool isActive, bool isEnabled);

    std::function<void()> onSelect;
    std::function<void()> onOpenPicker;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int slot;
    bool active = false, panelEnabled = true;
    juce::String name, info;
    CurveView curve;
    juce::Rectangle<int> nameArea;
};

// Round settings button with a gear icon.
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button ("Settings") {}
    void paintButton (juce::Graphics&, bool isOver, bool isDown) override;
};

//==============================================================================
class MixMateEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit MixMateEditor (MixMateProcessor&);
    ~MixMateEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

private:
    // Everything is laid out at 500 x 200 inside this, then scaled.
    class Content : public juce::Component
    {
    public:
        explicit Content (MixMateEditor& e) : editor (e) {}
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

        MixMateEditor& editor;
    };

    void timerCallback() override;
    void refreshFromProcessor (bool force);
    void applyScale (float scale);
    void layoutContent();
    void paintContent (juce::Graphics&);
    void focusForShortcuts();
    juce::String statusText (juce::Colour& colour) const;

    static constexpr int baseWidth = 500;
    static constexpr int baseHeight = 200;

    MixMateProcessor& processor;
    MixMateLookAndFeel lookAndFeel;
    Content content { *this };

    SegmentedControl modeControl { { "1", "2" } };
    SegmentedControl headphonesControl { { "Off", "HP 1", "HP 2" } };
    SegmentedControl monitorControl { { "Stereo", "Mid", "Side" } };
    SegmentedControl soloControl { { "Sub", "Bass", "Mid", "High" } };
    juce::Slider strengthSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> strengthAttachment;
    GearButton settingsButton;
    HeadphonePanel panels[2] { HeadphonePanel (0), HeadphonePanel (1) };
    HeadphonePicker picker;
    SettingsPanel settings;

    juce::Rectangle<int> centreArea, headphonesRow, monitorRow, soloRow, strengthRow, statusArea;

    // Last values shown, so the UI only repaints when something changes.
    int shownState[6] { -1, -1, -1, -1, -1, -1 };
    float shownStrength = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixMateEditor)
};

} // namespace mixmate
