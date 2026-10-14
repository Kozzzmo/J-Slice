#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/Types.h"

namespace ui
{
    //==============================================================================
    /** Colour skin. Every component reads pal() when painting, so a skin change is a repaint. */
    struct Palette
    {
        juce::String name;
        juce::Colour bg;        // window
        juce::Colour panel;     // section background
        juce::Colour inset;     // displays, wells
        juce::Colour raised;    // buttons, cells
        juce::Colour line;      // outlines
        juce::Colour faint;     // inactive dots, grid lines
        juce::Colour text;
        juce::Colour dim;
        juce::Colour accent;    // selected buttons
        juce::Colour onAccent;  // text on accent
        juce::Colour active;    // knob being edited, record-like highlights
        juce::Colour hot;       // alerts, important toggles
        juce::Colour kick, snare, hat, ghost;
    };

    int numSkins();
    int skinIndex();
    void setSkin (int index);
    const Palette& pal();

    inline juce::Colour classColour (bc::HitClass c)
    {
        switch (c)
        {
            case bc::HitClass::Kick:  return pal().kick;
            case bc::HitClass::Snare: return pal().snare;
            case bc::HitClass::Hat:   return pal().hat;
            case bc::HitClass::Ghost: return pal().ghost;
        }
        return pal().text;
    }

    //==============================================================================
    // Fonts : Instrument Serif (values, logo) + IBM Plex Mono (labels), both SIL OFL, embedded.
    juce::Font serif (float h);
    juce::Font serifItalic (float h);
    juce::Font mono (float h, bool bold = true, float tracking = 0.12f);

    /** Section background with a thin outline, like the hardware-style panels of Kinetik. */
    void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title = {});

    /** Small 4-point star used next to the logo. */
    void drawStar (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col);

    //==============================================================================
    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void applySkin();

        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getTextButtonFont (juce::TextButton&, int h) override;
        juce::Font getPopupMenuFont() override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
        void layoutFileBrowserComponent (juce::FileBrowserComponent&, juce::DirectoryContentsDisplayComponent*,
                                         juce::FilePreviewComponent*, juce::ComboBox* pathBox,
                                         juce::TextEditor* filenameBox, juce::Button* upButton) override;
        void drawFileBrowserRow (juce::Graphics&, int w, int h, const juce::File&, const juce::String& filename, juce::Image* icon,
                                 const juce::String& fileSizeDescription, const juce::String& fileTimeDescription,
                                 bool isDirectory, bool isItemSelected, int itemIndex, juce::DirectoryContentsDisplayComponent&) override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parentArea) override;
        void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
        void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
        void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                            int thumbStart, int thumbSize, bool over, bool down) override;
    };
}
