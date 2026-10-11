#include "LookAndFeel.h"
#include "BinaryData.h"

namespace ui
{

//==============================================================================
namespace
{
    std::vector<Palette> makeSkins()
    {
        using C = juce::Colour;
        std::vector<Palette> s;
        Palette neon;
        neon.name     = "NEON";
        neon.bg       = C (0xff0b0e27);
        neon.panel    = C (0xff0e1233);
        neon.inset    = C (0xff080a20);
        neon.raised   = C (0xff111538);
        neon.line     = C (0xff2e3577);
        neon.faint    = C (0xff272d63);
        neon.text     = C (0xffd3d6fb);
        neon.dim      = C (0xff6d74ab);
        neon.accent   = C (0xffaeb6ff);
        neon.onAccent = C (0xff10143a);
        neon.active   = C (0xffaeb6ff);
        neon.hot      = C (0xffff86c8);
        neon.kick     = C (0xffaeb6ff);
        neon.snare    = C (0xffff8cc8);
        neon.hat      = C (0xff7fe2d2);
        neon.ghost    = C (0xff8e7ff2);
        s.push_back (neon);

        Palette mid;
        mid.name     = "MIDNIGHT";
        mid.bg       = C (0xff2a3150);
        mid.panel    = C (0xff293050);
        mid.inset    = C (0xff212743);
        mid.raised   = C (0xff2b3253);
        mid.line     = C (0xff4f5884);
        mid.faint    = C (0xff444c75);
        mid.text     = C (0xffefe8d6);
        mid.dim      = C (0xff9198b5);
        mid.accent   = C (0xffefe6d2);
        mid.onAccent = C (0xff232946);
        mid.active   = C (0xfff2a44f);
        mid.hot      = C (0xffe8604c);
        mid.kick     = C (0xffe8604c);
        mid.snare    = C (0xfff2a44f);
        mid.hat      = C (0xff6fc7b8);
        mid.ghost    = C (0xffb9a8e6);
        s.push_back (mid);

        Palette amen;   // the original J-Slice look, kept as an option
        amen.name     = "AMEN";
        amen.bg       = C (0xff1f1e1b);
        amen.panel    = C (0xff2a2925);
        amen.inset    = C (0xff1a1916);
        amen.raised   = C (0xff35332e);
        amen.line     = C (0xff4c483f);
        amen.faint    = C (0xff433f37);
        amen.text     = C (0xffece4cd);
        amen.dim      = C (0xff9d9482);
        amen.accent   = C (0xffb5c77a);
        amen.onAccent = C (0xff1d2613);
        amen.active   = C (0xffff7a1a);
        amen.hot      = C (0xffff7a1a);
        amen.kick     = C (0xffff5a36);
        amen.snare    = C (0xffffc933);
        amen.hat      = C (0xff4fd3c4);
        amen.ghost    = C (0xffa58bff);
        s.push_back (amen);
        return s;
    }

    const std::vector<Palette>& skins()
    {
        static const std::vector<Palette> s = makeSkins();
        return s;
    }

    int currentSkin = 0;

    struct Fonts
    {
        juce::Typeface::Ptr serif, serifItalic, monoMedium, monoBold;
        Fonts()
        {
            serif       = juce::Typeface::createSystemTypefaceFor (BinaryData::InstrumentSerifRegular_ttf, BinaryData::InstrumentSerifRegular_ttfSize);
            serifItalic = juce::Typeface::createSystemTypefaceFor (BinaryData::InstrumentSerifItalic_ttf, BinaryData::InstrumentSerifItalic_ttfSize);
            monoMedium  = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize);
            monoBold    = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoSemiBold_ttf, BinaryData::IBMPlexMonoSemiBold_ttfSize);
        }
    };

    Fonts& fonts()
    {
        static Fonts f;
        return f;
    }
}

int numSkins()            { return (int) skins().size(); }
int skinIndex()           { return currentSkin; }
void setSkin (int index)  { currentSkin = juce::jlimit (0, numSkins() - 1, index); }
const Palette& pal()      { return skins()[(size_t) currentSkin]; }

juce::Font serif (float h)        { return juce::Font (juce::FontOptions (fonts().serif).withHeight (h)); }
juce::Font serifItalic (float h)  { return juce::Font (juce::FontOptions (fonts().serifItalic).withHeight (h)); }
juce::Font mono (float h, bool bold, float tracking)
{
    return juce::Font (juce::FontOptions (bold ? fonts().monoBold : fonts().monoMedium).withHeight (h).withKerningFactor (tracking));
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    auto rf = r.toFloat().reduced (0.5f);
    g.setColour (pal().panel);
    g.fillRoundedRectangle (rf, 6.0f);
    g.setColour (pal().line);
    g.drawRoundedRectangle (rf, 6.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setFont (mono (11.0f));
        g.setColour (pal().dim);
        g.drawText (title.toUpperCase(), r.getX() + 12, r.getY() + 6, r.getWidth() - 24, 14, juce::Justification::centredLeft);
    }
}

void drawStar (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col)
{
    juce::Path p;
    for (int i = 0; i < 8; ++i)
    {
        const float a = juce::MathConstants<float>::pi * 0.25f * (float) i - juce::MathConstants<float>::halfPi;
        const float rr = (i % 2 == 0) ? r : r * 0.28f;
        const auto pt = c + juce::Point<float> (std::cos (a), std::sin (a)) * rr;
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    p.closeSubPath();
    g.setColour (col);
    g.fillPath (p);
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    applySkin();
}

void LookAndFeel::applySkin()
{
    using namespace juce;
    const auto& p = pal();
    setColour (ResizableWindow::backgroundColourId, p.bg);
    setColour (Label::textColourId, p.text);
    setColour (Slider::textBoxTextColourId, p.text);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (TextButton::buttonColourId, p.raised);
    setColour (TextButton::buttonOnColourId, p.accent);
    setColour (TextButton::textColourOffId, p.text);
    setColour (TextButton::textColourOnId, p.onAccent);
    setColour (ComboBox::backgroundColourId, p.raised);
    setColour (ComboBox::textColourId, p.text);
    setColour (ComboBox::outlineColourId, p.line);
    setColour (ComboBox::arrowColourId, p.dim);
    setColour (PopupMenu::backgroundColourId, p.panel);
    setColour (PopupMenu::textColourId, p.text);
    setColour (PopupMenu::highlightedBackgroundColourId, p.accent);
    setColour (PopupMenu::highlightedTextColourId, p.onAccent);
    setColour (PopupMenu::headerTextColourId, p.dim);
    setColour (TextEditor::backgroundColourId, p.inset);
    setColour (TextEditor::textColourId, p.text);
    setColour (TextEditor::outlineColourId, p.line);
    setColour (TextEditor::focusedOutlineColourId, p.accent);
    setColour (TextEditor::highlightColourId, p.accent.withAlpha (0.35f));
    setColour (TextEditor::highlightedTextColourId, p.text);
    setColour (CaretComponent::caretColourId, p.accent);
    setColour (ListBox::backgroundColourId, p.inset);
    setColour (ListBox::textColourId, p.text);
    setColour (ListBox::outlineColourId, p.line);
    setColour (DirectoryContentsDisplayComponent::highlightColourId, p.accent);
    setColour (DirectoryContentsDisplayComponent::textColourId, p.text);
    setColour (DirectoryContentsDisplayComponent::highlightedTextColourId, p.onAccent);
    setColour (FileBrowserComponent::currentPathBoxBackgroundColourId, p.raised);
    setColour (FileBrowserComponent::currentPathBoxTextColourId, p.text);
    setColour (FileBrowserComponent::currentPathBoxArrowColourId, p.dim);
    setColour (FileBrowserComponent::filenameBoxBackgroundColourId, p.inset);
    setColour (FileBrowserComponent::filenameBoxTextColourId, p.text);
    setColour (ScrollBar::thumbColourId, p.line);
    setColour (ToggleButton::textColourId, p.text);
    setColour (ToggleButton::tickColourId, p.accent);
    setColour (TooltipWindow::backgroundColourId, p.panel);
    setColour (TooltipWindow::textColourId, p.text);
    setColour (TooltipWindow::outlineColourId, p.line);
    setColour (AlertWindow::backgroundColourId, p.panel);
    setColour (AlertWindow::textColourId, p.text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    using namespace juce;
    const auto& p = pal();
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float outer = size * 0.5f;
    const float angle = start + pos * (end - start);
    const bool hot = s.isMouseOverOrDragging() || s.isMouseButtonDown();
    const auto valueCol = s.isEnabled() ? (hot ? p.active : p.text) : p.dim;

    // ring of dots : lit up to the value
    const int dots = juce::jlimit (14, 28, (int) (size * 0.36f));
    const float dotR = jmax (1.1f, size * 0.022f);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float mid = (start + end) * 0.5f;
    for (int i = 0; i < dots; ++i)
    {
        const float a = start + (float) i / (float) (dots - 1) * (end - start);
        const bool lit = bipolar ? (a >= jmin (mid, angle) - 0.001f && a <= jmax (mid, angle) + 0.001f)
                                 : a <= angle + 0.001f;
        const auto pt = c.getPointOnCircumference (outer - dotR - 0.5f, a);
        g.setColour (lit ? valueCol : p.faint);
        g.fillEllipse (Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (pt));
    }

    // body
    const float bodyR = outer - dotR * 2.0f - jmax (3.0f, size * 0.07f);
    auto body = Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (c);
    g.setColour (p.inset);
    g.fillEllipse (body);
    g.setColour (hot ? p.active : p.line);
    g.drawEllipse (body, hot ? 1.6f : 1.2f);

    // pointer
    g.setColour (hot ? p.active : p.text);
    const auto p1 = c.getPointOnCircumference (bodyR * 0.18f, angle);
    const auto p2 = c.getPointOnCircumference (bodyR * 0.82f, angle);
    g.drawLine ({ p1, p2 }, jmax (1.8f, size * 0.035f));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace juce;
    const auto& p = pal();
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto fill = b.findColour (on ? TextButton::buttonOnColourId : TextButton::buttonColourId);
    auto outline = on ? fill : p.line;
    if (b.getProperties().contains ("hot") && ! on) outline = p.hot;
    if (! b.isEnabled()) { fill = fill.withMultipliedAlpha (0.5f); outline = outline.withMultipliedAlpha (0.5f); }
    if (over && ! on) { fill = fill.brighter (0.08f); outline = p.dim; }
    if (down) fill = fill.darker (0.12f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (r, 4.0f, b.getProperties().contains ("hot") ? 1.4f : 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    using namespace juce;
    const bool on = b.getToggleState();
    auto col = b.findColour (on ? TextButton::textColourOnId : TextButton::textColourOffId);
    if (b.getProperties().contains ("hot") && ! on) col = pal().hot;
    g.setColour (col.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (3, 1), Justification::centred, 2, 0.8f);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    using namespace juce;
    const auto& p = pal();
    auto r = b.getLocalBounds().toFloat();
    auto box = Rectangle<float> (14.0f, 14.0f).withCentre ({ r.getX() + 9.0f, r.getCentreY() });
    g.setColour (b.getToggleState() ? p.accent : p.inset);
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (b.getToggleState() ? p.accent : (over ? p.dim : p.line));
    g.drawRoundedRectangle (box, 3.0f, 1.0f);
    if (b.getToggleState())
    {
        Path tick;
        tick.startNewSubPath (box.getX() + 3.5f, box.getCentreY());
        tick.lineTo (box.getX() + 6.0f, box.getBottom() - 4.0f);
        tick.lineTo (box.getRight() - 3.0f, box.getY() + 3.5f);
        g.setColour (p.onAccent);
        g.strokePath (tick, PathStrokeType (1.8f));
    }
    g.setColour (p.text);
    g.setFont (mono (12.0f));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().withTrimmedLeft (22), Justification::centredLeft, 1);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    const auto& p = pal();
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (p.raised);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver (true) ? p.dim : p.line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 1.5f, ax + 3.5f, ay - 1.5f, ax, ay + 2.5f);
    g.setColour (p.dim.withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& b)          { return mono (juce::jmin (13.5f, b.getHeight() * 0.54f), true, 0.06f); }
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return mono (juce::jmin (12.5f, h * 0.5f)); }
juce::Font LookAndFeel::getPopupMenuFont()                           { return mono (14.0f, false, 0.03f); }

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setColour (juce::Label::textColourId, pal().text);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (pal().panel);
    g.setColour (pal().line);
    g.drawRect (0, 0, w, h, 1);
}

void LookAndFeel::layoutFileBrowserComponent (juce::FileBrowserComponent& b, juce::DirectoryContentsDisplayComponent* list,
                                              juce::FilePreviewComponent*, juce::ComboBox* pathBox,
                                              juce::TextEditor* filenameBox, juce::Button* upButton)
{
    auto r = b.getLocalBounds();
    auto top = r.removeFromTop (26);
    if (upButton != nullptr) upButton->setBounds (top.removeFromRight (36));
    top.removeFromRight (4);
    if (pathBox != nullptr) pathBox->setBounds (top);
    r.removeFromTop (4);
    if (filenameBox != nullptr) filenameBox->setVisible (false);
    if (auto* c = dynamic_cast<juce::Component*> (list)) c->setBounds (r);
}

void LookAndFeel::drawFileBrowserRow (juce::Graphics& g, int w, int h, const juce::File&, const juce::String& filename, juce::Image*,
                                      const juce::String&, const juce::String&, bool isDirectory, bool isItemSelected, int,
                                      juce::DirectoryContentsDisplayComponent&)
{
    using namespace juce;
    const auto& p = pal();
    if (isItemSelected)
    {
        g.setColour (p.accent);
        g.fillRoundedRectangle (Rectangle<float> (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f), 3.0f);
    }
    g.setColour (isItemSelected ? p.onAccent : (isDirectory ? p.dim : p.text));
    g.setFont (mono (13.5f, isDirectory, 0.02f));
    g.drawText ((isDirectory ? "/ " : "  ") + filename, 8, 0, w - 12, h, Justification::centredLeft, true);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    using namespace juce;
    g.fillAll (pal().panel);
    g.setColour (pal().line);
    g.drawRect (0, 0, w, h, 1);
    AttributedString s;
    s.setJustification (Justification::centredLeft);
    s.append (text, mono (13.0f, false, 0.02f), pal().text);
    TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, (float) w - 16.0f);
    tl.draw (g, Rectangle<float> (8.0f, 6.0f, (float) w - 16.0f, (float) h - 12.0f));
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parentArea)
{
    using namespace juce;
    AttributedString s;
    s.append (text, mono (13.0f, false, 0.02f));
    TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 420.0f);
    const int w = (int) tl.getWidth() + 18, h = (int) tl.getHeight() + 14;
    return Rectangle<int> (pos.x > parentArea.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                           pos.y > parentArea.getCentreY() ? pos.y - (h + 8) : pos.y + 8, w, h)
             .constrainedWithin (parentArea);
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (pal().inset);
    g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h), 4.0f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? pal().accent : pal().line);
    g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f), 4.0f, 1.0f);
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    auto r = vertical ? juce::Rectangle<int> (x + w / 2 - 2, thumbStart, 4, thumbSize)
                      : juce::Rectangle<int> (thumbStart, y + h / 2 - 2, thumbSize, 4);
    g.setColour ((over || down) ? pal().dim : pal().line);
    g.fillRoundedRectangle (r.toFloat(), 2.0f);
}

} // namespace ui
