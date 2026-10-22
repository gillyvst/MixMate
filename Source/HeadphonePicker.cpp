#include "HeadphonePicker.h"
#include "Theme.h"

namespace mixmate
{

namespace
{
    juce::String toSearchKey (const juce::String& s)
    {
        return s.toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789 ");
    }
}

HeadphonePicker::HeadphonePicker (const HeadphoneDatabase& db) : database (db)
{
    searchKeys.reserve ((size_t) database.size());
    for (int i = 0; i < database.size(); ++i)
        searchKeys.push_back (toSearchKey (database.get (i).name).removeCharacters (" "));

    title.setFont (MixMateLookAndFeel::font (14.0f, true));
    title.setColour (juce::Label::textColourId, Colours::text);
    addAndMakeVisible (title);

    search.setTextToShowWhenEmpty ("Search, e.g. \"hd 650\" or \"dt770\"", Colours::subtle);
    search.setFont (MixMateLookAndFeel::font (13.0f));
    search.setIndents (8, 0);
    search.setJustification (juce::Justification::centredLeft);
    search.onTextChange = [this] { rebuildRows(); };
    search.onReturnKey = [this] { choose (juce::jmax (0, list.getSelectedRow())); };
    search.onEscapeKey = [this] { close(); };
    search.addKeyListener (this);
    addAndMakeVisible (search);

    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (closeButton);

    list.setModel (this);
    list.setRowHeight (20);
    list.setOutlineThickness (1);
    list.addKeyListener (this);
    addAndMakeVisible (list);
}

void HeadphonePicker::open (int newSlot, int newCurrentIndex)
{
    slot = newSlot;
    currentIndex = newCurrentIndex;
    title.setText ("Choose headphone " + juce::String (slot + 1), juce::dontSendNotification);
    search.clear();
    rebuildRows();

    // Start on the current headphone in the A-Z part of the list.
    for (int r = (int) rows.size(); --r >= 0;)
        if (rows[(size_t) r].index == currentIndex)
        {
            list.selectRow (r);
            list.scrollToEnsureRowIsOnscreen (juce::jmin ((int) rows.size() - 1, r + 3));
            break;
        }

    setVisible (true);
    toFront (false);
    search.grabKeyboardFocus();
}

void HeadphonePicker::close()
{
    setVisible (false);
    if (onClose)
        onClose();
}

void HeadphonePicker::rebuildRows()
{
    rows.clear();
    const auto& popular = database.getPopular();

    juce::StringArray tokens;
    tokens.addTokens (toSearchKey (search.getText()), " ", {});
    tokens.removeEmptyStrings();

    if (tokens.isEmpty())
    {
        rows.push_back ({ true, -1, "Popular" });
        for (auto index : popular)
            rows.push_back ({ false, index, {} });

        rows.push_back ({ true, -1, "All headphones A-Z (" + juce::String (database.size()) + ")" });
        for (int i = 0; i < database.size(); ++i)
            rows.push_back ({ false, i, {} });
    }
    else
    {
        auto matches = [&] (int index)
        {
            for (const auto& t : tokens)
                if (! searchKeys[(size_t) index].contains (t))
                    return false;
            return true;
        };

        std::vector<Row> found;
        for (auto index : popular)
            if (matches (index))
                found.push_back ({ false, index, {} });

        for (int i = 0; i < database.size(); ++i)
            if (matches (i) && std::find (popular.begin(), popular.end(), i) == popular.end())
                found.push_back ({ false, i, {} });

        const auto count = (int) found.size();
        rows.push_back ({ true, -1, count == 0 ? juce::String ("No matches")
                                               : juce::String (count) + (count == 1 ? " match" : " matches") });
        rows.insert (rows.end(), found.begin(), found.end());
    }

    list.updateContent();
    list.repaint();
    list.selectRow (rows.size() > 1 ? 1 : -1);
    list.scrollToEnsureRowIsOnscreen (0);
}

void HeadphonePicker::choose (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;

    const auto& r = rows[(size_t) row];
    if (r.isHeader)
        return;

    if (onChoose)
        onChoose (slot, r.index);
    close();
}

void HeadphonePicker::moveSelection (int delta)
{
    const int numRows = (int) rows.size();
    int row = list.getSelectedRow();

    do
        row += delta;
    while (juce::isPositiveAndBelow (row, numRows) && rows[(size_t) row].isHeader);

    if (juce::isPositiveAndBelow (row, numRows))
    {
        list.selectRow (row);
        list.scrollToEnsureRowIsOnscreen (row);
    }
}

bool HeadphonePicker::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (key == juce::KeyPress::upKey)       { moveSelection (-1); return true; }
    if (key == juce::KeyPress::downKey)     { moveSelection (1); return true; }
    if (key == juce::KeyPress::pageUpKey)   { moveSelection (-8); return true; }
    if (key == juce::KeyPress::pageDownKey) { moveSelection (8); return true; }
    if (key == juce::KeyPress::escapeKey)   { close(); return true; }
    if (key == juce::KeyPress::returnKey)   { choose (list.getSelectedRow()); return true; }
    return false;
}

int HeadphonePicker::getNumRows()
{
    return (int) rows.size();
}

void HeadphonePicker::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) rows.size()))
        return;

    const auto& r = rows[(size_t) row];
    auto area = juce::Rectangle<int> (width, height).reduced (10, 0);

    if (r.isHeader)
    {
        g.setColour (Colours::background);
        g.fillRect (0, 0, width, height);
        g.setColour (Colours::subtle);
        g.setFont (MixMateLookAndFeel::font (10.5f, true));
        g.drawText (r.text.toUpperCase(), area, juce::Justification::centredLeft);
        return;
    }

    const auto& hp = database.get (r.index);

    if (selected)
    {
        g.setColour (Colours::accentLight);
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) width, (float) height).reduced (3.0f, 1.0f), 4.0f);
    }

    auto info = hp.type + "  " + hp.source;
    g.setFont (MixMateLookAndFeel::font (10.0f));
    const int infoWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), info) + 4;
    g.setColour (Colours::subtle);
    g.drawText (info, area.removeFromRight (infoWidth), juce::Justification::centredRight);

    g.setColour (r.index == currentIndex ? Colours::accentDark : Colours::text);
    g.setFont (MixMateLookAndFeel::font (12.5f, r.index == currentIndex));
    g.drawText (hp.name, area.withTrimmedRight (8), juce::Justification::centredLeft, true);
}

void HeadphonePicker::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    choose (row);
}

void HeadphonePicker::returnKeyPressed (int row)
{
    choose (row);
}

void HeadphonePicker::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
}

void HeadphonePicker::resized()
{
    auto area = getLocalBounds().reduced (12, 10);

    auto top = area.removeFromTop (26);
    closeButton.setBounds (top.removeFromRight (64));
    top.removeFromRight (8);
    title.setBounds (top.removeFromLeft (150));
    search.setBounds (top);

    area.removeFromTop (8);
    list.setBounds (area);
}

} // namespace mixmate
