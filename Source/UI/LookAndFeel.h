#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Core/Types.h"

namespace ui
{
    namespace col
    {
        const juce::Colour body      { 0xff24231f };
        const juce::Colour panel     { 0xff302e29 };
        const juce::Colour panelEdge { 0xff171613 };
        const juce::Colour pad       { 0xff46433c };
        const juce::Colour padHi     { 0xff5a564d };
        const juce::Colour cream     { 0xffe9e1cb };
        const juce::Colour dim       { 0xff9a9282 };
        const juce::Colour accent    { 0xffff7a1a };
        const juce::Colour amber     { 0xffffb000 };
        const juce::Colour lcdBg     { 0xffa7ba74 };
        const juce::Colour lcdBg2    { 0xff94a865 };
        const juce::Colour lcdInk    { 0xff1d2613 };
        const juce::Colour kick      { 0xffff5a36 };
        const juce::Colour snare     { 0xffffc933 };
        const juce::Colour hat       { 0xff4fd3c4 };
        const juce::Colour ghost     { 0xffa58bff };
    }

    inline juce::Colour classColour (bc::HitClass c)
    {
        switch (c)
        {
            case bc::HitClass::Kick:  return col::kick;
            case bc::HitClass::Snare: return col::snare;
            case bc::HitClass::Hat:   return col::hat;
            case bc::HitClass::Ghost: return col::ghost;
        }
        return col::cream;
    }

    inline juce::Font monoFont (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font labelFont (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
        void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                               juce::Slider::SliderStyle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        juce::Font getTextButtonFont (juce::TextButton&, int h) override;
        juce::Font getLabelFont (juce::Label&) override;
        juce::Font getPopupMenuFont() override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
        void layoutFileBrowserComponent (juce::FileBrowserComponent&, juce::DirectoryContentsDisplayComponent*,
                                         juce::FilePreviewComponent*, juce::ComboBox* pathBox,
                                         juce::TextEditor* filenameBox, juce::Button* upButton) override;
    };

    /** Panel with a silk-screened title, like a hardware sampler section. */
    void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title);
}
