#include "PluginEditor.h"

namespace mixmate
{

//==============================================================================
SegmentedControl::SegmentedControl (juce::StringArray l)
    : labels (std::move (l)),
      enabled ((size_t) labels.size(), true),
      fills ((size_t) labels.size(), Colours::accent)
{
}

void SegmentedControl::setSelected (int index)
{
    if (selected != index)
    {
        selected = index;
        repaint();
    }
}

void SegmentedControl::setSegmentEnabled (int index, bool shouldBeEnabled)
{
    if (enabled[(size_t) index] != shouldBeEnabled)
    {
        enabled[(size_t) index] = shouldBeEnabled;
        repaint();
    }
}

void SegmentedControl::setHighlight (int index, juce::Colour fill, juce::Colour)
{
    fills[(size_t) index] = fill;
}

int SegmentedControl::segmentAt (juce::Point<float> p) const
{
    if (labels.isEmpty() || ! getLocalBounds().toFloat().contains (p))
        return -1;
    return juce::jlimit (0, labels.size() - 1, (int) (p.x / ((float) getWidth() / (float) labels.size())));
}

void SegmentedControl::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const float corner = 6.0f;

    g.setColour (Colours::background);
    g.fillRoundedRectangle (bounds, corner);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (bounds, corner, 1.0f);

    const float w = bounds.getWidth() / (float) labels.size();
    for (int i = 0; i < labels.size(); ++i)
    {
        auto seg = juce::Rectangle<float> (bounds.getX() + w * (float) i, bounds.getY(), w, bounds.getHeight());
        const bool isEnabled = enabled[(size_t) i];

        if (i == selected)
        {
            g.setColour (fills[(size_t) i]);
            g.fillRoundedRectangle (seg.reduced (2.0f), corner - 2.0f);
        }
        else if (i == hovered && isEnabled)
        {
            g.setColour (Colours::accent.withAlpha (0.10f));
            g.fillRoundedRectangle (seg.reduced (2.0f), corner - 2.0f);
        }

        g.setColour (i == selected ? juce::Colours::white
                                   : (isEnabled ? Colours::text : Colours::subtle.withAlpha (0.5f)));
        g.setFont (MixMateLookAndFeel::font (11.0f, i == selected));
        g.drawText (labels[i], seg, juce::Justification::centred);
    }
}

void SegmentedControl::mouseDown (const juce::MouseEvent& e)
{
    const int seg = segmentAt (e.position);
    if (seg >= 0 && enabled[(size_t) seg] && onClick)
        onClick (seg);
}

void SegmentedControl::mouseMove (const juce::MouseEvent& e)
{
    const int seg = segmentAt (e.position);
    if (seg != hovered)
    {
        hovered = seg;
        repaint();
    }
}

void SegmentedControl::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

//==============================================================================
namespace
{
    constexpr int curvePoints = 96;
    constexpr float curveRangeDb = 12.0f;

    float frequencyAt (int i)
    {
        return 20.0f * std::pow (1000.0f, (float) i / (float) (curvePoints - 1));   // 20 Hz to 20 kHz
    }

    float xForFrequency (float freq, juce::Rectangle<float> area)
    {
        return area.getX() + area.getWidth() * std::log (freq / 20.0f) / std::log (1000.0f);
    }
}

void CurveView::setCurve (const EqCurve& curve, float strength)
{
    responseDb.resize (curvePoints);
    for (int i = 0; i < curvePoints; ++i)
        responseDb[(size_t) i] = (float) curveResponseDb (curve, frequencyAt (i), strength);
    repaint();
}

void CurveView::setActive (bool shouldBeActive)
{
    if (active != shouldBeActive)
    {
        active = shouldBeActive;
        repaint();
    }
}

void CurveView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Colours::background);
    g.fillRoundedRectangle (bounds, 6.0f);

    auto plot = bounds.reduced (4.0f, 6.0f);
    auto yFor = [plot] (float db)
    {
        return juce::jmap (juce::jlimit (-curveRangeDb, curveRangeDb, db), curveRangeDb, -curveRangeDb, plot.getY(), plot.getBottom());
    };

    // Grid: 100 Hz, 1 kHz, 10 kHz and 0 dB.
    g.setColour (Colours::border.withAlpha (0.7f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.fillRect (juce::Rectangle<float> (xForFrequency (f, plot), plot.getY(), 1.0f, plot.getHeight()));
    g.setColour (Colours::border);
    g.fillRect (juce::Rectangle<float> (plot.getX(), yFor (0.0f) - 0.5f, plot.getWidth(), 1.0f));

    g.setFont (MixMateLookAndFeel::font (8.0f));
    g.setColour (Colours::subtle.withAlpha (0.8f));
    const char* gridLabels[] = { "100", "1k", "10k" };
    const float gridFreqs[] = { 100.0f, 1000.0f, 10000.0f };
    for (int i = 0; i < 3; ++i)
        g.drawText (gridLabels[i], juce::Rectangle<float> (xForFrequency (gridFreqs[i], plot) + 2.0f, plot.getBottom() - 10.0f, 24.0f, 10.0f),
                    juce::Justification::left);

    if (responseDb.empty())
        return;

    juce::Path line, fill;
    for (int i = 0; i < curvePoints; ++i)
    {
        const float x = xForFrequency (frequencyAt (i), plot);
        const float y = yFor (responseDb[(size_t) i]);
        if (i == 0) line.startNewSubPath (x, y);
        else        line.lineTo (x, y);
    }

    fill = line;
    fill.lineTo (plot.getRight(), yFor (0.0f));
    fill.lineTo (plot.getX(), yFor (0.0f));
    fill.closeSubPath();

    const auto colour = active ? Colours::accent : Colours::subtle;
    g.setColour (colour.withAlpha (active ? 0.18f : 0.10f));
    g.fillPath (fill);
    g.setColour (colour);
    g.strokePath (line, juce::PathStrokeType (active ? 1.8f : 1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void CurveView::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasDraggedSinceMouseDown() && onClick)
        onClick();
}

//==============================================================================
HeadphonePanel::HeadphonePanel (int s) : slot (s)
{
    curve.onClick = [this] { if (onSelect) onSelect(); };
    addAndMakeVisible (curve);
}

void HeadphonePanel::update (const Headphone& hp, float strength, bool isActive, bool isEnabled)
{
    name = hp.name;
    info = hp.type + "  " + hp.source;
    active = isActive;
    panelEnabled = isEnabled;
    curve.setCurve (hp.curve, strength);
    curve.setActive (isActive);
    setAlpha (isEnabled ? 1.0f : 0.5f);
    repaint();
}

void HeadphonePanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.75f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (bounds, 10.0f);
    g.setColour (active ? Colours::accent : Colours::border);
    g.drawRoundedRectangle (bounds, 10.0f, active ? 1.5f : 1.0f);

    auto area = getLocalBounds().reduced (10, 7);
    auto titleRow = area.removeFromTop (14);

    g.setFont (MixMateLookAndFeel::font (9.5f, true));
    g.setColour (active ? Colours::accentDark : Colours::subtle);
    g.drawText ("HEADPHONE " + juce::String (slot + 1), titleRow, juce::Justification::centredLeft);

    if (active)
    {
        auto pill = titleRow.removeFromRight (26).toFloat().reduced (0.0f, 1.0f);
        g.setColour (Colours::accent);
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (juce::Colours::white);
        g.setFont (MixMateLookAndFeel::font (8.5f, true));
        g.drawText ("ON", pill, juce::Justification::centred);
    }
    else if (! panelEnabled)
    {
        g.setFont (MixMateLookAndFeel::font (8.5f));
        g.setColour (Colours::subtle);
        g.drawText ("not in use", titleRow, juce::Justification::centredRight);
    }

    // Name box, opens the list.
    auto box = nameArea.toFloat().reduced (0.5f);
    g.setColour (Colours::background);
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (box, 6.0f, 1.0f);

    auto textArea = nameArea.reduced (8, 0);
    auto chevronArea = textArea.removeFromRight (10).toFloat();
    juce::Path chevron;
    const auto c = chevronArea.getCentre();
    chevron.startNewSubPath (c.x - 3.5f, c.y - 1.5f);
    chevron.lineTo (c.x, c.y + 2.0f);
    chevron.lineTo (c.x + 3.5f, c.y - 1.5f);
    g.setColour (Colours::accentDark);
    g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (Colours::text);
    g.setFont (MixMateLookAndFeel::font (11.5f, true));
    g.drawFittedText (name, textArea.withTrimmedRight (4), juce::Justification::centredLeft, 2, 0.8f);

    g.setColour (Colours::subtle);
    g.setFont (MixMateLookAndFeel::font (9.0f));
    g.drawText (info, area.removeFromBottom (12), juce::Justification::centredLeft, true);
}

void HeadphonePanel::resized()
{
    auto area = getLocalBounds().reduced (10, 7);
    area.removeFromTop (17);
    area.removeFromBottom (13);
    nameArea = area.removeFromBottom (26);
    area.removeFromBottom (5);
    curve.setBounds (area);
}

void HeadphonePanel::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown())
        return;

    if (nameArea.contains (e.getPosition()))
    {
        if (onOpenPicker) onOpenPicker();
    }
    else if (onSelect)
    {
        onSelect();
    }
}

//==============================================================================
void GearButton::paintButton (juce::Graphics& g, bool isOver, bool isDown)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (isDown ? Colours::accentLight.darker (0.05f) : (isOver ? Colours::accentLight : Colours::panel));
    g.fillEllipse (bounds);
    g.setColour (isOver ? Colours::accent : Colours::border);
    g.drawEllipse (bounds, 1.0f);

    const auto c = bounds.getCentre();
    const float outer = bounds.getWidth() * 0.32f, inner = outer * 0.72f;
    juce::Path gear;
    constexpr int teeth = 8;
    for (int i = 0; i < teeth * 2; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi * (float) i / (teeth * 2);
        const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / (teeth * 2);
        const float r = (i % 2 == 0) ? outer : inner;
        const auto p0 = c.getPointOnCircumference (r, a0);
        const auto p1 = c.getPointOnCircumference (r, a1);
        if (i == 0) gear.startNewSubPath (p0);
        else        gear.lineTo (p0);
        gear.lineTo (p1);
    }
    gear.closeSubPath();
    gear.addEllipse (c.x - inner * 0.42f, c.y - inner * 0.42f, inner * 0.84f, inner * 0.84f);
    gear.setUsingNonZeroWinding (false);

    g.setColour (isOver ? Colours::accentDark : Colours::subtle);
    g.fillPath (gear);
}

//==============================================================================
MixMateEditor::MixMateEditor (MixMateProcessor& p)
    : AudioProcessorEditor (p), processor (p), picker (*p.database), settings (p)
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);
    addAndMakeVisible (content);

    // Header.
    modeControl.onClick = [this] (int index) { processor.setParameter (ParamIDs::twoMode, (float) index); };
    content.addAndMakeVisible (modeControl);

    settingsButton.setWantsKeyboardFocus (false);
    settingsButton.onClick = [this] { settings.open(); };
    content.addAndMakeVisible (settingsButton);

    // Centre controls.
    headphonesControl.onClick = [this] (int index)
    {
        processor.setParameter (ParamIDs::headphones, (float) index);
    };
    monitorControl.onClick = [this] (int index) { processor.setParameter (ParamIDs::monitor, (float) index); };
    monitorControl.setHighlight (monitorMid, Colours::warm, Colours::warmLight);
    monitorControl.setHighlight (monitorSide, Colours::warm, Colours::warmLight);

    soloControl.onClick = [this] (int index)
    {
        // Clicking the soloed band again turns solo off.
        const int band = index + 1;
        processor.setParameter (ParamIDs::solo, (float) (processor.getChoice (ParamIDs::solo) == band ? soloOff : band));
    };
    for (int i = 0; i < 4; ++i)
        soloControl.setHighlight (i, Colours::warm, Colours::warmLight);

    for (auto* c : { &headphonesControl, &monitorControl, &soloControl })
        content.addAndMakeVisible (c);

    strengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    strengthSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    strengthSlider.setWantsKeyboardFocus (false);
    strengthSlider.setDoubleClickReturnValue (true, 100.0);
    content.addAndMakeVisible (strengthSlider);
    strengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, ParamIDs::strength, strengthSlider);

    // Headphone panels.
    for (int slot = 0; slot < 2; ++slot)
    {
        panels[slot].onSelect = [this, slot]
        {
            if (slot == 1 && processor.getChoice (ParamIDs::twoMode) == 0)
                processor.setParameter (ParamIDs::twoMode, 1.0f);
            processor.setParameter (ParamIDs::headphones, (float) (slot + 1));
        };
        panels[slot].onOpenPicker = [this, slot] { picker.open (slot, processor.getHeadphoneIndex (slot)); };
        content.addAndMakeVisible (panels[slot]);
    }

    // Overlays.
    picker.onChoose = [this] (int slot, int index) { processor.setHeadphone (slot, index); refreshFromProcessor (true); };
    picker.onClose = [this] { focusForShortcuts(); };
    content.addChildComponent (picker);

    settings.onClose = [this] { focusForShortcuts(); };
    settings.onScaleChanged = [this] (float scale) { applyScale (scale); };
    settings.onKeysChanged = [this] { content.repaint(); };
    content.addChildComponent (settings);

    applyScale (processor.getUiScale());
    refreshFromProcessor (true);
    startTimerHz (30);
}

MixMateEditor::~MixMateEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MixMateEditor::applyScale (float scale)
{
    content.setBounds (0, 0, baseWidth, baseHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));
}

void MixMateEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
}

void MixMateEditor::resized()
{
    content.setBounds (0, 0, baseWidth, baseHeight);
}

void MixMateEditor::visibilityChanged()
{
    focusForShortcuts();
}

void MixMateEditor::focusForShortcuts()
{
    if (isShowing() && ! picker.isVisible())
        grabKeyboardFocus();
}

bool MixMateEditor::keyPressed (const juce::KeyPress& key)
{
    if (settings.isLearningKey())
    {
        settings.learnKey (key);
        return true;
    }

    if (picker.isVisible())
        return false;

    for (auto action : { CycleAction::headphones, CycleAction::monitor, CycleAction::solo })
        if (key == processor.getKey (action))
        {
            processor.cycle (action);
            refreshFromProcessor (false);
            return true;
        }

    if (key == juce::KeyPress::escapeKey && settings.isVisible())
    {
        settings.close();
        return true;
    }

    return false;   // let the DAW have everything else
}

void MixMateEditor::timerCallback()
{
    refreshFromProcessor (false);
}

void MixMateEditor::refreshFromProcessor (bool force)
{
    const int state[6] = {
        processor.getChoice (ParamIDs::twoMode),
        processor.getChoice (ParamIDs::headphones),
        processor.getChoice (ParamIDs::monitor),
        processor.getChoice (ParamIDs::solo),
        processor.getHeadphoneIndex (0),
        processor.getHeadphoneIndex (1),
    };
    const float strength = processor.apvts.getRawParameterValue (ParamIDs::strength)->load() / 100.0f;

    if (! force && std::equal (std::begin (state), std::end (state), std::begin (shownState)) && juce::exactlyEqual (strength, shownStrength))
        return;

    std::copy (std::begin (state), std::end (state), std::begin (shownState));
    shownStrength = strength;

    const bool twoMode = state[0] != 0;
    int hpState = state[1];
    if (! twoMode && hpState == headphones2)
        hpState = headphones1;

    modeControl.setSelected (twoMode ? 1 : 0);
    headphonesControl.setSelected (hpState);
    headphonesControl.setSegmentEnabled (headphones2, twoMode);
    monitorControl.setSelected (state[2]);
    soloControl.setSelected (state[3] - 1);

    for (int slot = 0; slot < 2; ++slot)
    {
        const int index = state[4 + slot];
        if (index >= 0)
            panels[slot].update (processor.database->get (index), strength,
                                 hpState == slot + 1, slot == 0 || twoMode);
    }

    content.repaint();
}

juce::String MixMateEditor::statusText (juce::Colour& colour) const
{
    const bool twoMode = shownState[0] != 0;
    int hpState = shownState[1];
    if (! twoMode && hpState == headphones2)
        hpState = headphones1;

    juce::StringArray parts;
    if (hpState == headphonesOff)
    {
        parts.add ("Speakers - no correction");
        colour = Colours::subtle;
    }
    else
    {
        const int index = shownState[3 + hpState];
        parts.add ("Correcting " + (index >= 0 ? processor.database->get (index).name : juce::String()));
        colour = Colours::accentDark;
    }

    const char* monitorNames[] = { "", "Mid", "Side" };
    const char* soloNames[] = { "", "Sub", "Bass", "Mids", "Highs" };
    if (shownState[2] != monitorStereo || shownState[3] != soloOff)
    {
        colour = Colours::warm.darker (0.25f);
        if (shownState[2] != monitorStereo) parts.insert (0, monitorNames[shownState[2]]);
        if (shownState[3] != soloOff)       parts.insert (0, juce::String (soloNames[shownState[3]]) + " solo");
    }

    return parts.joinIntoString ("  |  ");
}

//==============================================================================
void MixMateEditor::Content::paint (juce::Graphics& g)
{
    editor.paintContent (g);
}

void MixMateEditor::Content::resized()
{
    editor.layoutContent();
}

void MixMateEditor::Content::mouseDown (const juce::MouseEvent&)
{
    editor.focusForShortcuts();
}

void MixMateEditor::layoutContent()
{
    modeControl.setBounds (388, 9, 54, 20);
    settingsButton.setBounds (456, 7, 26, 26);
    statusArea = { 120, 10, 214, 18 };

    panels[0].setBounds (10, 38, 156, 154);
    centreArea = { 172, 38, 156, 154 };
    panels[1].setBounds (334, 38, 156, 154);

    auto inner = centreArea.reduced (9, 7);
    auto takeRow = [&inner] (int controlHeight)
    {
        auto row = inner.removeFromTop (13 + controlHeight);
        inner.removeFromTop (5);
        return row;
    };

    headphonesRow = takeRow (22);
    monitorRow = takeRow (22);
    soloRow = takeRow (22);
    strengthRow = inner;

    headphonesControl.setBounds (headphonesRow.withTrimmedTop (13));
    monitorControl.setBounds (monitorRow.withTrimmedTop (13));
    soloControl.setBounds (soloRow.withTrimmedTop (13));
    strengthSlider.setBounds (strengthRow.withTrimmedTop (13).withHeight (16).expanded (2, 0));

    picker.setBounds (0, 0, baseWidth, baseHeight);
    settings.setBounds (0, 0, baseWidth, baseHeight);
}

void MixMateEditor::paintContent (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    // Title.
    g.setColour (Colours::text);
    const auto titleFont = MixMateLookAndFeel::font (18.0f, true);
    const int mixWidth = juce::GlyphArrangement::getStringWidthInt (titleFont, "Mix");
    g.setFont (titleFont);
    g.drawText ("Mix", juce::Rectangle<int> (16, 7, mixWidth + 2, 24), juce::Justification::centredLeft);
    g.setColour (Colours::accent);
    g.drawText ("Mate", juce::Rectangle<int> (16 + mixWidth, 7, 70, 24), juce::Justification::centredLeft);

    // Status pill.
    juce::Colour statusColour;
    const auto status = statusText (statusColour);
    g.setColour (statusColour.withAlpha (0.10f));
    g.fillRoundedRectangle (statusArea.toFloat(), 9.0f);
    g.setColour (statusColour);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ (float) statusArea.getX() + 10.0f, (float) statusArea.getCentreY() }));
    g.setFont (MixMateLookAndFeel::font (10.0f, true));
    g.drawFittedText (status, statusArea.withTrimmedLeft (18).withTrimmedRight (8), juce::Justification::centredLeft, 1, 0.8f);

    g.setColour (Colours::subtle);
    g.setFont (MixMateLookAndFeel::font (9.5f));
    g.drawText ("Mode", juce::Rectangle<int> (354, 9, 32, 20), juce::Justification::centredLeft);

    // Centre panel.
    auto centre = centreArea.toFloat().reduced (0.75f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (centre, 10.0f);
    g.setColour (Colours::border);
    g.drawRoundedRectangle (centre, 10.0f, 1.0f);

    auto drawRowLabel = [&] (juce::Rectangle<int> row, const juce::String& text, const juce::String& keyText)
    {
        auto labelArea = row.removeFromTop (12);
        g.setColour (Colours::subtle);
        g.setFont (MixMateLookAndFeel::font (9.5f, true));
        g.drawText (text, labelArea, juce::Justification::centredLeft);

        if (keyText.isNotEmpty())
        {
            g.setFont (MixMateLookAndFeel::font (8.5f, true));
            const float w = juce::jmax (14.0f, (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), keyText) + 8.0f);
            auto cap = labelArea.toFloat().removeFromRight (w).reduced (0.0f, 0.5f);
            g.setColour (Colours::background);
            g.fillRoundedRectangle (cap, 3.0f);
            g.setColour (Colours::border);
            g.drawRoundedRectangle (cap, 3.0f, 1.0f);
            g.setColour (Colours::accentDark);
            g.drawText (keyText, cap, juce::Justification::centred);
        }
    };

    auto keyText = [this] (CycleAction a) { return processor.getKey (a).getTextDescription().toUpperCase(); };
    drawRowLabel (headphonesRow, "HEADPHONES", keyText (CycleAction::headphones));
    drawRowLabel (monitorRow, "MID / SIDE", keyText (CycleAction::monitor));
    drawRowLabel (soloRow, "SOLO", keyText (CycleAction::solo));

    auto strengthLabel = strengthRow.withHeight (12);
    drawRowLabel (strengthLabel, "STRENGTH", {});
    g.setColour (Colours::accentDark);
    g.setFont (MixMateLookAndFeel::font (9.5f, true));
    g.drawText (juce::String (juce::roundToInt (shownStrength * 100.0f)) + "%", strengthLabel, juce::Justification::centredRight);
}

} // namespace mixmate
