#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "HeadphoneDatabase.h"

namespace mixmate
{

// Full-window overlay with a search box and a list of every headphone:
// the popular ones first, then everything A-Z.
class HeadphonePicker : public juce::Component,
                        private juce::ListBoxModel,
                        private juce::KeyListener
{
public:
    explicit HeadphonePicker (const HeadphoneDatabase& db);

    void open (int slot, int currentIndex);
    void close();

    std::function<void (int slot, int databaseIndex)> onChoose;
    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        bool isHeader = false;
        int index = -1;          // database index, or -1 for headers
        juce::String text;       // header text
    };

    void rebuildRows();
    void choose (int row);
    void moveSelection (int delta);

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;

    const HeadphoneDatabase& database;
    std::vector<juce::String> searchKeys;   // lower case, letters and digits only
    std::vector<Row> rows;
    int slot = 0;
    int currentIndex = -1;

    juce::Label title;
    juce::TextEditor search;
    juce::TextButton closeButton { "Close" };
    juce::ListBox list;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeadphonePicker)
};

} // namespace mixmate
