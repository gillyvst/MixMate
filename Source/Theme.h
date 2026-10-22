#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace mixmate
{

namespace Colours
{
    inline const juce::Colour background  { 0xfff8f3ea };   // warm cream
    inline const juce::Colour panel       { 0xfffffcf7 };
    inline const juce::Colour border      { 0xffe7ddcc };
    inline const juce::Colour track       { 0xffefe7da };
    inline const juce::Colour text        { 0xff3b4150 };
    inline const juce::Colour subtle      { 0xff959aa5 };
    inline const juce::Colour accent      { 0xff6f9bd4 };   // soft blue
    inline const juce::Colour accentDark  { 0xff4f7cb8 };
    inline const juce::Colour accentLight { 0xffe2ecf8 };
    inline const juce::Colour warm        { 0xffe9a46a };   // used for solo / mid-side "on"
    inline const juce::Colour warmLight   { 0xfffbeadb };
}

class MixMateLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MixMateLookAndFeel()
    {
        regular = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsRegular_ttf, BinaryData::PoppinsRegular_ttfSize);
        semiBold = juce::Typeface::createSystemTypefaceFor (BinaryData::PoppinsSemiBold_ttf, BinaryData::PoppinsSemiBold_ttfSize);

        setColour (juce::ResizableWindow::backgroundColourId, Colours::background);
        setColour (juce::Label::textColourId, Colours::text);
        setColour (juce::TextEditor::backgroundColourId, Colours::panel);
        setColour (juce::TextEditor::textColourId, Colours::text);
        setColour (juce::TextEditor::outlineColourId, Colours::border);
        setColour (juce::TextEditor::focusedOutlineColourId, Colours::accent);
        setColour (juce::TextEditor::highlightColourId, Colours::accentLight);
        setColour (juce::TextEditor::highlightedTextColourId, Colours::text);
        setColour (juce::CaretComponent::caretColourId, Colours::accentDark);
        setColour (juce::ListBox::backgroundColourId, Colours::panel);
        setColour (juce::ListBox::outlineColourId, Colours::border);
        setColour (juce::ScrollBar::thumbColourId, Colours::border);
        setColour (juce::Slider::thumbColourId, Colours::accentDark);
        setColour (juce::Slider::trackColourId, Colours::accent);
        setColour (juce::Slider::backgroundColourId, Colours::track);
        setColour (juce::TextButton::buttonColourId, Colours::panel);
        setColour (juce::TextButton::buttonOnColourId, Colours::accent);
        setColour (juce::TextButton::textColourOffId, Colours::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ToggleButton::textColourId, Colours::text);
        setColour (juce::ToggleButton::tickColourId, Colours::accentDark);
        setColour (juce::ToggleButton::tickDisabledColourId, Colours::subtle);
    }

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override
    {
        return font.isBold() ? semiBold : regular;
    }

    static juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return font (juce::jmin (13.0f, (float) buttonHeight * 0.55f));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool isOver, bool isDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto fill = button.getToggleState() ? Colours::accent : Colours::panel;
        if (isDown)       fill = fill.darker (0.06f);
        else if (isOver)  fill = fill.brighter (0.03f).overlaidWith (Colours::accent.withAlpha (0.06f));

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (button.getToggleState() ? Colours::accent : Colours::border);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isOver, bool) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto box = bounds.removeFromLeft (bounds.getHeight()).reduced (3.5f);

        g.setColour (button.getToggleState() ? Colours::accent : Colours::panel);
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (button.getToggleState() ? Colours::accent : (isOver ? Colours::accent : Colours::border));
        g.drawRoundedRectangle (box, 4.0f, 1.0f);

        if (button.getToggleState())
        {
            juce::Path tick;
            tick.startNewSubPath (box.getX() + box.getWidth() * 0.25f, box.getCentreY());
            tick.lineTo (box.getX() + box.getWidth() * 0.43f, box.getBottom() - box.getHeight() * 0.28f);
            tick.lineTo (box.getRight() - box.getWidth() * 0.22f, box.getY() + box.getHeight() * 0.28f);
            g.setColour (juce::Colours::white);
            g.strokePath (tick, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour (Colours::text);
        g.setFont (font (12.0f));
        g.drawText (button.getButtonText(), bounds.withTrimmedLeft (4.0f), juce::Justification::centredLeft);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        const auto trackHeight = 4.0f;
        const auto cy = (float) y + (float) height * 0.5f;
        juce::Rectangle<float> track ((float) x, cy - trackHeight * 0.5f, (float) width, trackHeight);

        g.setColour (Colours::track);
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (slider.isEnabled() ? Colours::accent : Colours::subtle);
        g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);

        const float r = 6.0f;
        g.setColour (juce::Colours::white);
        g.fillEllipse (sliderPos - r, cy - r, r * 2, r * 2);
        g.setColour (slider.isEnabled() ? Colours::accentDark : Colours::subtle);
        g.drawEllipse (sliderPos - r, cy - r, r * 2, r * 2, 1.5f);
    }

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isVertical, int thumbStart, int thumbSize, bool isOver, bool) override
    {
        juce::Rectangle<int> thumb = isVertical ? juce::Rectangle<int> (x, thumbStart, width, thumbSize)
                                                : juce::Rectangle<int> (thumbStart, y, thumbSize, height);
        g.setColour (isOver ? Colours::subtle.withAlpha (0.6f) : Colours::border);
        g.fillRoundedRectangle (thumb.toFloat().reduced (2.5f, 1.0f), 2.5f);
    }

    int getDefaultScrollbarWidth() override { return 9; }

private:
    juce::Typeface::Ptr regular, semiBold;
};

} // namespace mixmate
