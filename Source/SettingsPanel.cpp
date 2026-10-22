#include "SettingsPanel.h"
#include "Theme.h"

namespace mixmate
{

namespace
{
    const CycleAction keyActions[3] = { CycleAction::headphones, CycleAction::monitor, CycleAction::solo };
    const float scales[3] = { 1.0f, 1.25f, 1.5f };

    void styleHeading (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (MixMateLookAndFeel::font (10.0f, true));
        label.setColour (juce::Label::textColourId, Colours::subtle);
    }

    void styleLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setFont (MixMateLookAndFeel::font (12.0f));
        label.setColour (juce::Label::textColourId, Colours::text);
    }
}

SettingsPanel::SettingsPanel (MixMateProcessor& p) : processor (p)
{
    title.setText ("Settings", juce::dontSendNotification);
    title.setFont (MixMateLookAndFeel::font (15.0f, true));
    addAndMakeVisible (title);

    styleHeading (xoverHeading, "SOLO CROSSOVERS");
    styleHeading (keysHeading, "SHORTCUT KEYS");
    styleHeading (sizeHeading, "SIZE");
    for (auto* l : { &xoverHeading, &keysHeading, &sizeHeading })
        addAndMakeVisible (l);

    const char* xoverNames[3] = { "Sub / Bass", "Bass / Mids", "Mids / Highs" };
    const char* xoverIDs[3] = { ParamIDs::xSub, ParamIDs::xBass, ParamIDs::xHigh };
    for (int i = 0; i < 3; ++i)
    {
        styleLabel (xoverLabels[i], xoverNames[i]);
        addAndMakeVisible (xoverLabels[i]);

        auto& s = xoverSliders[i];
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
        s.setColour (juce::Slider::textBoxTextColourId, Colours::text);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxHighlightColourId, Colours::accentLight);
        addAndMakeVisible (s);
        xoverAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, xoverIDs[i], s);
    }

    const char* keyNames[3] = { "Headphones", "Mid / Side", "Solo band" };
    for (int i = 0; i < 3; ++i)
    {
        styleLabel (keyLabels[i], keyNames[i]);
        addAndMakeVisible (keyLabels[i]);

        keyButtons[i].setWantsKeyboardFocus (false);
        keyButtons[i].onClick = [this, i]
        {
            learning = (learning == i) ? -1 : i;
            refresh();
        };
        addAndMakeVisible (keyButtons[i]);
    }

    levelMatchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, ParamIDs::levelMatch, levelMatchToggle);
    exportToggle.onClick = [this] { processor.setBypassWhenExporting (exportToggle.getToggleState()); };
    for (auto* t : { &levelMatchToggle, &exportToggle })
    {
        t->setWantsKeyboardFocus (false);
        addAndMakeVisible (t);
    }

    for (int i = 0; i < 3; ++i)
    {
        sizeButtons[i].setButtonText (juce::String (juce::roundToInt (scales[i] * 100)) + "%");
        sizeButtons[i].setWantsKeyboardFocus (false);
        sizeButtons[i].onClick = [this, i]
        {
            processor.setUiScale (scales[i]);
            refresh();
            if (onScaleChanged)
                onScaleChanged (scales[i]);
        };
        addAndMakeVisible (sizeButtons[i]);
    }

    saveDefaultButton.onClick = [this]
    {
        processor.saveAsDefault();
        saveDefaultButton.setButtonText ("Saved!");
        juce::Timer::callAfterDelay (1500, [safe = juce::Component::SafePointer<SettingsPanel> (this)]
        {
            if (safe != nullptr)
                safe->saveDefaultButton.setButtonText ("Save as default");
        });
    };

    resetButton.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Reset MixMate?",
            "This puts every setting back to how MixMate shipped and removes your saved default.",
            "Reset", "Cancel", this,
            juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<SettingsPanel> (this)] (int result)
            {
                if (result == 1 && safe != nullptr)
                {
                    safe->processor.resetToFactory();
                    safe->refresh();
                    if (safe->onScaleChanged)  safe->onScaleChanged (safe->processor.getUiScale());
                    if (safe->onKeysChanged)   safe->onKeysChanged();
                }
            }));
    };

    doneButton.onClick = [this] { close(); };
    for (auto* b : { &saveDefaultButton, &resetButton, &doneButton })
    {
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    credits.setText ("Headphone curves from AutoEQ by Jaakko Pasanen (MIT licence). Font: Poppins (OFL).",
                     juce::dontSendNotification);
    credits.setFont (MixMateLookAndFeel::font (9.5f));
    credits.setColour (juce::Label::textColourId, Colours::subtle);
    credits.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (credits);
}

void SettingsPanel::open()
{
    learning = -1;
    refresh();
    setVisible (true);
    toFront (false);
}

void SettingsPanel::close()
{
    learning = -1;
    setVisible (false);
    if (onClose)
        onClose();
}

void SettingsPanel::learnKey (const juce::KeyPress& key)
{
    if (learning < 0)
        return;

    if (key != juce::KeyPress::escapeKey)
    {
        processor.setKey (keyActions[learning], key);
        if (onKeysChanged)
            onKeysChanged();
    }

    learning = -1;
    refresh();
}

void SettingsPanel::refresh()
{
    for (int i = 0; i < 3; ++i)
    {
        const bool isLearning = learning == i;
        keyButtons[i].setButtonText (isLearning ? "Press a key..." : processor.getKey (keyActions[i]).getTextDescription());
        keyButtons[i].setToggleState (isLearning, juce::dontSendNotification);
    }

    const float scale = processor.getUiScale();
    for (int i = 0; i < 3; ++i)
        sizeButtons[i].setToggleState (std::abs (scale - scales[i]) < 0.01f, juce::dontSendNotification);

    exportToggle.setToggleState (processor.getBypassWhenExporting(), juce::dontSendNotification);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    auto divider = getLocalBounds().toFloat().withTrimmedTop (40).withTrimmedBottom (26);
    g.setColour (Colours::border);
    g.fillRect (juce::Rectangle<float> (divider.getCentreX() - 0.5f, divider.getY(), 1.0f, divider.getHeight()));
}

void SettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (14, 8);

    auto top = area.removeFromTop (26);
    doneButton.setBounds (top.removeFromRight (64));
    title.setBounds (top);

    credits.setBounds (area.removeFromBottom (16));
    area.removeFromTop (6);

    auto left = area.removeFromLeft (area.getWidth() / 2).withTrimmedRight (14);
    auto right = area.withTrimmedLeft (14);

    xoverHeading.setBounds (left.removeFromTop (16));
    for (int i = 0; i < 3; ++i)
    {
        auto row = left.removeFromTop (22);
        xoverLabels[i].setBounds (row.removeFromLeft (82));
        xoverSliders[i].setBounds (row);
    }
    left.removeFromTop (6);
    levelMatchToggle.setBounds (left.removeFromTop (22));
    exportToggle.setBounds (left.removeFromTop (22));

    keysHeading.setBounds (right.removeFromTop (16));
    for (int i = 0; i < 3; ++i)
    {
        auto row = right.removeFromTop (22);
        keyLabels[i].setBounds (row.removeFromLeft (100));
        keyButtons[i].setBounds (row.removeFromLeft (110).reduced (0, 1));
    }
    right.removeFromTop (6);

    auto sizeRow = right.removeFromTop (22);
    sizeHeading.setBounds (sizeRow.removeFromLeft (44));
    for (auto& b : sizeButtons)
    {
        b.setBounds (sizeRow.removeFromLeft (52).reduced (2, 1));
    }

    right.removeFromTop (4);
    auto buttonRow = right.removeFromTop (22);
    saveDefaultButton.setBounds (buttonRow.removeFromLeft (124).reduced (2, 0));
    resetButton.setBounds (buttonRow.removeFromLeft (70).reduced (2, 0));
}

} // namespace mixmate
