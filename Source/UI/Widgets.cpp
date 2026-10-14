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
    slider.setPopupDisplayEnabled (false, false, nullptr);
}

Knob::~Knob() { slider.removeListener (this); }

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (46);
    const int s = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto text = r.removeFromBottom (46);
    const bool hot = slider.isMouseOverOrDragging();
    g.setFont (mono (11.5f));
    g.setColour (hot ? pal().active : pal().text);
    g.drawText (caption, text.removeFromTop (17), juce::Justification::centred);

    juce::String v;
    if (format) v = format (slider.getValue());
    else if (param != nullptr) v = param->getCurrentValueAsText();
    g.setFont (serif (24.0f));
    g.setColour (hot ? pal().active : pal().text);
    g.drawText (v, text, juce::Justification::centredTop);
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
