#include "LookAndFeel.h"

namespace ui
{

LookAndFeel::LookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, col::body);
    setColour (Label::textColourId, col::cream);
    setColour (Slider::textBoxTextColourId, col::amber);
    setColour (Slider::textBoxBackgroundColourId, col::panelEdge);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::rotarySliderFillColourId, col::accent);
    setColour (Slider::trackColourId, col::accent);
    setColour (TextButton::buttonColourId, col::pad);
    setColour (TextButton::buttonOnColourId, col::accent);
    setColour (TextButton::textColourOffId, col::cream);
    setColour (TextButton::textColourOnId, col::body);
    setColour (ComboBox::backgroundColourId, col::panelEdge);
    setColour (ComboBox::textColourId, col::amber);
    setColour (ComboBox::outlineColourId, Colour (0xff4a463e));
    setColour (ComboBox::arrowColourId, col::amber);
    setColour (PopupMenu::backgroundColourId, Colour (0xff1e1d1a));
    setColour (PopupMenu::textColourId, col::cream);
    setColour (PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (PopupMenu::highlightedTextColourId, col::body);
    setColour (PopupMenu::headerTextColourId, col::amber);
    setColour (TextEditor::backgroundColourId, col::panelEdge);
    setColour (TextEditor::textColourId, col::amber);
    setColour (TextEditor::outlineColourId, Colour (0xff4a463e));
    setColour (TextEditor::focusedOutlineColourId, col::accent);
    setColour (TextEditor::highlightColourId, col::accent.withAlpha (0.4f));
    setColour (CaretComponent::caretColourId, col::amber);
    setColour (ListBox::backgroundColourId, Colour (0xff1e1d1a));
    setColour (ListBox::textColourId, col::cream);
    setColour (DirectoryContentsDisplayComponent::highlightColourId, col::accent);
    setColour (DirectoryContentsDisplayComponent::textColourId, col::cream);
    setColour (DirectoryContentsDisplayComponent::highlightedTextColourId, col::body);
    setColour (FileBrowserComponent::currentPathBoxBackgroundColourId, col::panelEdge);
    setColour (FileBrowserComponent::currentPathBoxTextColourId, col::amber);
    setColour (FileBrowserComponent::currentPathBoxArrowColourId, col::amber);
    setColour (FileBrowserComponent::filenameBoxBackgroundColourId, col::panelEdge);
    setColour (FileBrowserComponent::filenameBoxTextColourId, col::amber);
    setColour (ScrollBar::thumbColourId, col::padHi);
    setColour (ToggleButton::textColourId, col::cream);
    setColour (ToggleButton::tickColourId, col::accent);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff1e1d1a));
    setColour (TooltipWindow::textColourId, col::cream);
    setColour (TooltipWindow::outlineColourId, col::accent);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    using namespace juce;
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    auto r = bounds.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float radius = size * 0.5f;
    const float angle = start + pos * (end - start);
    const bool enabled = s.isEnabled();

    // tick marks
    g.setColour (col::dim.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = start + (float) i / 10.0f * (end - start);
        const auto p1 = c.getPointOnCircumference (radius - 1.0f, a);
        const auto p2 = c.getPointOnCircumference (radius - (i % 5 == 0 ? 5.0f : 3.0f), a);
        g.drawLine ({ p1, p2 }, 1.0f);
    }

    // value arc
    const float arcR = radius - 6.5f;
    Path bg, arc;
    bg.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (col::panelEdge);
    g.strokePath (bg, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
    g.setColour (enabled ? s.findColour (Slider::rotarySliderFillColourId) : col::dim);
    g.strokePath (arc, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // cap
    const float capR = arcR - 4.5f;
    auto cap = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillEllipse (cap.translated (0.0f, 1.5f));
    ColourGradient grad (Colour (0xfff1ead6), cap.getX(), cap.getY(), Colour (0xffb9b09a), cap.getRight(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (cap);
    g.setColour (Colour (0xff7d7564));
    g.drawEllipse (cap, 1.0f);

    // pointer
    g.setColour (enabled ? col::body : col::dim);
    const auto p1 = c.getPointOnCircumference (capR * 0.25f, angle);
    const auto p2 = c.getPointOnCircumference (capR * 0.9f, angle);
    g.drawLine ({ p1, p2 }, 2.4f);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace juce;
    if (style != Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    auto track = Rectangle<float> ((float) x, (float) y + h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (col::panelEdge);
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (col::accent);
    g.fillRoundedRectangle (track.withRight (pos), 3.0f);
    auto thumb = Rectangle<float> (10.0f, (float) h * 0.8f).withCentre ({ pos, (float) y + h * 0.5f });
    g.setColour (col::cream);
    g.fillRoundedRectangle (thumb, 2.0f);
    g.setColour (col::body);
    g.drawLine (thumb.getCentreX(), thumb.getY() + 3, thumb.getCentreX(), thumb.getBottom() - 3, 1.5f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    auto base = b.findColour (on ? TextButton::buttonOnColourId : TextButton::buttonColourId);
    if (! b.isEnabled()) base = base.withMultipliedSaturation (0.3f).darker (0.3f);
    if (over) base = base.brighter (0.12f);
    if (down) base = base.darker (0.2f);

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.0f, down ? 0.5f : 2.0f), 4.0f);
    ColourGradient grad (base.brighter (0.15f), r.getX(), r.getY(), base.darker (0.15f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 0.0f), 4.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    using namespace juce;
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.findColour (b.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId)
                  .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2).translated (0, down ? 1 : 0), Justification::centred, 2);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat();
    auto led = Rectangle<float> (12.0f, 12.0f).withCentre ({ r.getX() + 9.0f, r.getCentreY() });
    const bool on = b.getToggleState();
    g.setColour (col::panelEdge);
    g.fillEllipse (led.expanded (2.0f));
    if (on)
    {
        g.setColour (col::accent.withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (on ? col::accent : Colour (0xff4a2a14));
    g.fillEllipse (led);
    g.setColour (over ? col::cream : col::cream.withAlpha (0.85f));
    g.setFont (labelFont (12.5f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (22), Justification::centredLeft, 1);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (box.findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 3.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (ComboBox::arrowColourId).withMultipliedAlpha (box.isEnabled() ? 1.0f : 0.4f));
    g.fillPath (arrow);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& b)       { return monoFont (juce::jmin (14.0f, b.getHeight() * 0.55f)); }
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return labelFont (juce::jmin (13.5f, h * 0.48f)); }
juce::Font LookAndFeel::getLabelFont (juce::Label& l)             { return l.getFont(); }
juce::Font LookAndFeel::getPopupMenuFont()                        { return labelFont (14.0f, false); }

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void LookAndFeel::layoutFileBrowserComponent (juce::FileBrowserComponent& b, juce::DirectoryContentsDisplayComponent* list,
                                               juce::FilePreviewComponent*, juce::ComboBox* pathBox,
                                               juce::TextEditor* filenameBox, juce::Button* upButton)
{
    auto r = b.getLocalBounds();
    auto top = r.removeFromTop (26);
    if (upButton != nullptr) upButton->setBounds (top.removeFromRight (40));
    top.removeFromRight (4);
    if (pathBox != nullptr) pathBox->setBounds (top);
    r.removeFromTop (4);
    if (filenameBox != nullptr) filenameBox->setVisible (false);
    if (auto* c = dynamic_cast<juce::Component*> (list)) c->setBounds (r);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    using namespace juce;
    auto rf = r.toFloat();
    g.setColour (col::panelEdge);
    g.fillRoundedRectangle (rf, 6.0f);
    g.setColour (col::panel);
    g.fillRoundedRectangle (rf.reduced (1.5f), 5.0f);
    // screws
    g.setColour (Colour (0xff57534a));
    for (auto p : { rf.getTopLeft() + Point<float> (7, 7), rf.getTopRight() + Point<float> (-7, 7),
                    rf.getBottomLeft() + Point<float> (7, -7), rf.getBottomRight() + Point<float> (-7, -7) })
        g.fillEllipse (Rectangle<float> (5, 5).withCentre (p));
    if (title.isNotEmpty())
    {
        g.setFont (labelFont (11.0f));
        g.setColour (col::accent);
        g.drawText (title.toUpperCase(), r.getX() + 16, r.getY() + 4, r.getWidth() - 32, 14, Justification::centredLeft);
    }
}

} // namespace ui
