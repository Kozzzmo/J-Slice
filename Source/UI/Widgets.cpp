#include "Widgets.h"

namespace ui
{

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& cap)
    : caption (cap.toUpperCase())
{
    param = s.getParameter (paramId);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    if (param != nullptr)
        slider.setDoubleClickReturnValue (true, (double) param->convertFrom0to1 (param->getDefaultValue()));
    slider.addListener (this);
    slider.addMouseListener (this, false);
    slider.setPopupDisplayEnabled (false, false, nullptr);
}

Knob::~Knob() { slider.removeMouseListener (this); slider.removeListener (this); }

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (20);
    const int sz = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (sz, sz));
}

void Knob::paint (juce::Graphics&) {}

void Knob::paintOverChildren (juce::Graphics& g)
{
    // the value in the centre of the disc, the caption under the knob
    const bool hot = slider.isMouseOverOrDragging() || slider.isMouseButtonDown();
    auto sb = slider.getBounds().toFloat();
    const float d = juce::jmin (sb.getWidth(), sb.getHeight());
    juce::String v;
    if (format) v = format (slider.getValue());
    else if (param != nullptr) v = param->getCurrentValueAsText();
    g.setFont (serif (juce::jlimit (13.0f, 30.0f, d * 0.34f)));
    g.setColour (hot ? pal().hot : pal().text);
    g.drawFittedText (v, sb.withSizeKeepingCentre (d * 0.62f, d * 0.5f).toNearestInt(), juce::Justification::centred, 1, 0.6f);

    g.setFont (mono (11.0f));
    g.setColour (hot ? pal().hot : pal().dim);
    g.drawText (caption, getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

//==============================================================================
ValueBox::ValueBox (const juce::String& l) : label (l.toUpperCase())
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void ValueBox::attachTo (juce::RangedAudioParameter& p, std::function<juce::String (double)> fmt)
{
    auto* param = &p;
    const auto& range = p.getNormalisableRange();
    minValue = range.start;
    maxValue = range.end;
    interval = range.interval;
    get = [param] { return (double) param->convertFrom0to1 (param->getValue()); };
    set = [param] (double v) { param->setValueNotifyingHost (param->convertTo0to1 ((float) v)); };
    onGestureStart = [param] { param->beginChangeGesture(); };
    onGestureEnd = [param] { param->endChangeGesture(); };
    format = fmt ? fmt : [param] (double) { return param->getCurrentValueAsText(); };
}

double ValueBox::snap (double v) const
{
    v = juce::jlimit (minValue, maxValue, v);
    if (interval > 0.0) v = minValue + std::round ((v - minValue) / interval) * interval;
    return juce::jlimit (minValue, maxValue, v);
}

void ValueBox::refresh()
{
    if (! get) return;
    const auto t = format ? format (get()) : juce::String (get(), 2);
    if (t != lastText) repaint();
}

void ValueBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool hot = isMouseOverOrDragging() || dragging;
    g.setColour (pal().inset);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (hot ? pal().dim : pal().line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    auto inner = getLocalBounds().reduced (6, 3);
    g.setFont (mono (10.5f));
    g.setColour (pal().dim);
    g.drawText (label, inner.removeFromTop (13), large ? juce::Justification::centredLeft : juce::Justification::centred);

    const double v = get ? get() : 0.0;
    lastText = format ? format (v) : juce::String (v, 2);
    if (showBar) inner.removeFromBottom (7);
    g.setFont (serif (large ? 26.0f : juce::jmin (26.0f, (float) inner.getHeight() + 6.0f)));
    g.setColour (dragging ? pal().active : pal().text);
    g.drawFittedText (lastText, inner, large ? juce::Justification::centredLeft : juce::Justification::centred, 1, 0.7f);

    if (showBar)
    {
        auto bar = getLocalBounds().reduced (8, 0).removeFromBottom (9).withTrimmedBottom (4).toFloat();
        g.setColour (pal().faint);
        for (float x = bar.getX(); x < bar.getRight(); x += 3.0f)
            g.fillRect (x, bar.getCentreY() - 0.5f, 1.2f, 1.2f);
        g.setColour (pal().line);
        g.drawRect (bar, 0.8f);
        const double norm = maxValue > minValue ? (v - minValue) / (maxValue - minValue) : 0.0;
        const float x = bar.getX() + (float) juce::jlimit (0.0, 1.0, norm) * bar.getWidth();
        g.setColour (dragging ? pal().active : pal().text);
        g.fillRect (x - 1.0f, bar.getY() - 1.0f, 2.0f, bar.getHeight() + 2.0f);
    }
}

void ValueBox::mouseDown (const juce::MouseEvent&)
{
    if (! get || ! set) return;
    dragStartValue = get();
    dragging = true;
    if (onGestureStart) onGestureStart();
    repaint();
}

void ValueBox::mouseDrag (const juce::MouseEvent& e)
{
    if (! set) return;
    const double delta = (double) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
    const double speed = e.mods.isShiftDown() ? 0.15 : 1.0;
    set (snap (dragStartValue + delta / pixelsForFullRange * (maxValue - minValue) * speed));
    repaint();
}

void ValueBox::mouseUp (const juce::MouseEvent&)
{
    if (dragging && onGestureEnd) onGestureEnd();
    dragging = false;
    repaint();
}

void ValueBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (! get || ! set) return;
    const double step = interval > 0.0 ? interval : (maxValue - minValue) / 100.0;
    const double dir = (w.deltaY + w.deltaX) > 0.0f ? 1.0 : -1.0;
    if (onGestureStart) onGestureStart();
    set (snap (get() + dir * step));
    if (onGestureEnd) onGestureEnd();
    repaint();
}

void ValueBox::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! editable || ! set) return;
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (serif (20.0f));
    editor->setJustification (juce::Justification::centred);
    editor->setText (juce::String (get ? get() : 0.0, interval >= 1.0 ? 0 : 2), false);
    editor->selectAll();
    addAndMakeVisible (*editor);
    editor->setBounds (getLocalBounds().reduced (2));
    editor->grabKeyboardFocus();
    auto commit = [this]
    {
        if (editor == nullptr) return;
        const auto txt = editor->getText().retainCharacters ("0123456789.-");
        if (txt.isNotEmpty())
        {
            if (onGestureStart) onGestureStart();
            set (snap (txt.getDoubleValue()));
            if (onGestureEnd) onGestureEnd();
        }
        juce::MessageManager::callAsync ([safe = SafePointer<ValueBox> (this)] { if (safe != nullptr) { safe->editor.reset(); safe->repaint(); } });
    };
    editor->onReturnKey = commit;
    editor->onFocusLost = commit;
    editor->onEscapeKey = [this] { juce::MessageManager::callAsync ([safe = SafePointer<ValueBox> (this)] { if (safe != nullptr) safe->editor.reset(); }); };
}

//==============================================================================
HBar::HBar (const juce::String& c) : caption (c.toUpperCase())
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

juce::Rectangle<float> HBar::barArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromLeft (caption.isNotEmpty() ? 96.0f : 0.0f);
    r.removeFromRight (58.0f);
    return r.withSizeKeepingCentre (r.getWidth(), 12.0f);
}

void HBar::paint (juce::Graphics& g)
{
    const bool en = isEnabled();
    const double v = get ? juce::jlimit (0.0, 1.0, get()) : 0.0;
    auto r = getLocalBounds();
    g.setFont (mono (11.0f));
    g.setColour (pal().dim.withMultipliedAlpha (en ? 1.0f : 0.5f));
    if (caption.isNotEmpty()) g.drawText (caption, r.removeFromLeft (96), juce::Justification::centredLeft);
    g.setFont (serif (24.0f));
    g.setColour ((dragging ? pal().hot : pal().text).withMultipliedAlpha (en ? 1.0f : 0.4f));
    g.drawText (format ? format (v) : juce::String (juce::roundToInt (v * 100.0)), r.removeFromRight (54), juce::Justification::centredRight);

    auto b = barArea();
    g.setColour (pal().inset);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (pal().line);
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
    if (en)
    {
        auto fillR = b.reduced (2.0f);
        fillR.setWidth (juce::jmax (fillR.getHeight(), fillR.getWidth() * (float) v));
        g.setColour (dragging ? pal().hot : (colour.isTransparent() ? pal().accent : colour));
        g.fillRoundedRectangle (fillR, 4.0f);
    }
}

void HBar::setFromX (float x)
{
    auto b = barArea();
    const double v = juce::jlimit (0.0, 1.0, (double) ((x - b.getX()) / juce::jmax (1.0f, b.getWidth())));
    if (set) set (v, first);
    first = false;
    repaint();
}

void HBar::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    first = true;
    dragging = true;
    setFromX (e.position.x);
}

void HBar::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging) setFromX (e.position.x);
}

void HBar::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    if (onRelease) onRelease();
    repaint();
}

void HBar::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (! isEnabled() || ! get || ! set) return;
    const double dir = (w.deltaY + w.deltaX) > 0.0f ? 1.0 : -1.0;
    set (juce::jlimit (0.0, 1.0, get() + dir * 0.05), true);
    repaint();
}

//==============================================================================
ZoneFader::ZoneFader (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& c, juce::StringArray z)
    : caption (c.toUpperCase()), zones (std::move (z))
{
    param = s.getParameter (paramId);
    if (param != nullptr) param->addListener (this);
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

ZoneFader::~ZoneFader()
{
    if (param != nullptr) param->removeListener (this);
}

juce::Rectangle<float> ZoneFader::track() const
{
    return { 9.0f, 25.0f, (float) getWidth() - 18.0f, 12.0f };
}

float ZoneFader::valueForX (float x, bool detent) const
{
    auto t = track();
    float v = juce::jlimit (0.0f, 1.0f, (x - t.getX()) / juce::jmax (1.0f, t.getWidth()));
    if (detent && zones.size() > 1)
    {
        // gentle magnet on the zone centres : easy to land exactly on a named setting
        const float stepV = 1.0f / (float) (zones.size() - 1);
        const float nearest = std::round (v / stepV) * stepV;
        if (std::abs (v - nearest) < 0.025f) v = nearest;
    }
    return v;
}

void ZoneFader::setValue (float v)
{
    if (param != nullptr) param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    repaint();
}

void ZoneFader::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto& p = pal();
    const float v = param != nullptr ? param->getValue() : 0.0f;
    const bool hot = dragging || isMouseOver();
    const int n = zones.size();
    const int zone = n > 1 ? jlimit (0, n - 1, roundToInt (v * (float) (n - 1))) : 0;

    g.setFont (mono (12.0f, true, 0.16f));
    g.setColour (p.text);
    g.drawText (caption, 2, 0, getWidth() / 2, 20, Justification::centredLeft);
    g.setFont (serif (24.0f));
    g.setColour (dragging ? p.hot : p.text);
    const int shown = roundToInt ((v - 0.5f) * 200.0f);   // centre = the style as is
    g.drawText ((shown > 0 ? "+" : "") + String (shown), getWidth() / 2, -2, getWidth() / 2 - 2, 24, Justification::centredRight);

    auto t = track();
    g.setColour (p.inset);
    g.fillRoundedRectangle (t, 6.0f);
    g.setColour (p.line);
    g.drawRoundedRectangle (t.reduced (0.5f), 6.0f, 1.0f);
    // zone ticks (the centre one is drawn brighter, with the fill)
    for (int i = 1; i < n - 1; ++i)
    {
        const float x = t.getX() + t.getWidth() * (float) i / (float) (n - 1);
        g.setColour (p.faint);
        g.fillRect (x - 0.5f, t.getY() + 3.0f, 1.0f, t.getHeight() - 6.0f);
    }
    // filled from the centre (the style as is) towards the thumb
    {
        auto inner = t.reduced (2.0f);
        const float cx = inner.getX() + inner.getWidth() * 0.5f, vx = inner.getX() + inner.getWidth() * v;
        g.setColour (dragging ? p.hot : p.accent);
        g.fillRoundedRectangle (Rectangle<float> (jmin (cx, vx), inner.getY(), jmax (2.0f, std::abs (vx - cx)), inner.getHeight()), 3.0f);
        g.setColour (p.dim);
        g.fillRect (t.getCentreX() - 0.75f, t.getY() - 3.0f, 1.5f, t.getHeight() + 6.0f);
    }

    // thumb
    const float tx = t.getX() + t.getWidth() * v;
    auto thumb = Rectangle<float> (18.0f, 18.0f).withCentre ({ jlimit (t.getX() + 6.0f, t.getRight() - 6.0f, tx), t.getCentreY() });
    g.setColour (hot ? p.hot : p.text);
    g.fillEllipse (thumb);
    g.setColour (p.bg);
    g.drawEllipse (thumb.reduced (1.0f), 2.0f);

    // zone names : the current one is lit
    g.setFont (mono (10.5f, true, 0.0f));
    for (int i = 0; i < n; ++i)
    {
        const float cx = t.getX() + t.getWidth() * (float) i / (float) jmax (1, n - 1);
        Rectangle<float> lr (70.0f, 16.0f);
        lr = lr.withCentre ({ cx, t.getBottom() + 12.0f });
        if (i == 0) lr.setX (t.getX() - 2.0f);
        if (i == n - 1) lr.setRight (t.getRight() + 2.0f);
        const auto just = i == 0 ? Justification::centredLeft : (i == n - 1 ? Justification::centredRight : Justification::centred);
        g.setColour (i == zone ? p.text : p.dim.withAlpha (0.8f));
        g.drawText (zones[i], lr.toNearestInt(), just);
    }
}

void ZoneFader::mouseDown (const juce::MouseEvent& e)
{
    if (param == nullptr) return;
    auto t = track();
    if (e.position.y > t.getBottom() + 3.0f && zones.size() > 1)
    {
        // click on a zone name : jump to it
        const float v = valueForX (e.position.x, false);
        const float stepV = 1.0f / (float) (zones.size() - 1);
        param->beginChangeGesture();
        setValue (std::round (v / stepV) * stepV);
        param->endChangeGesture();
        return;
    }
    dragging = true;
    dragStartV = param->getValue();
    param->beginChangeGesture();
    // click on the track jumps, click on the thumb only grabs
    const float tx = t.getX() + t.getWidth() * dragStartV;
    if (std::abs (e.position.x - tx) > 10.0f) { setValue (valueForX (e.position.x, true)); dragStartV = param->getValue(); }
}

void ZoneFader::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const float speed = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const float dv = (float) e.getDistanceFromDragStartX() / juce::jmax (1.0f, track().getWidth()) * speed;
    auto t = track();
    setValue (valueForX (t.getX() + t.getWidth() * (dragStartV + dv), ! e.mods.isShiftDown()));
}

void ZoneFader::mouseUp (const juce::MouseEvent&)
{
    if (dragging && param != nullptr) param->endChangeGesture();
    dragging = false;
    repaint();
}

void ZoneFader::mouseDoubleClick (const juce::MouseEvent&)
{
    if (param == nullptr) return;
    param->beginChangeGesture();
    setValue (param->getDefaultValue());
    param->endChangeGesture();
}

void ZoneFader::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (param == nullptr) return;
    const float dir = (w.deltaY + w.deltaX) > 0.0f ? 1.0f : -1.0f;
    param->beginChangeGesture();
    setValue (param->getValue() + dir * 0.02f);
    param->endChangeGesture();
}

//==============================================================================
ParamToggle::ParamToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& text)
    : juce::TextButton (text)
{
    setClickingTogglesState (true);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, paramId, *this);
}

//==============================================================================
ParamChoice::ParamChoice (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& cap)
    : caption (cap.toUpperCase())
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (paramId)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, paramId, box);
}

void ParamChoice::paint (juce::Graphics& g)
{
    if (caption.isEmpty()) return;
    g.setFont (mono (11.0f));
    g.setColour (pal().dim);
    g.drawText (caption, getLocalBounds().removeFromTop (16), juce::Justification::centredLeft);
}

void ParamChoice::resized()
{
    auto r = getLocalBounds();
    if (caption.isNotEmpty()) r.removeFromTop (17);
    box.setBounds (r.removeFromTop (juce::jmin (28, r.getHeight())));
}

} // namespace ui
