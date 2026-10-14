#pragma once

#include "LookAndFeel.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{

/** "Palet" knob : solid disc with the value written in the centre, thin value arc, caption under it. */
struct Knob : public juce::Component, private juce::Slider::Listener
{
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& caption);
    ~Knob() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseUp (const juce::MouseEvent&) override    { repaint(); }

    juce::Slider slider;
    juce::String caption;
    std::function<juce::String (double)> format;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::RangedAudioParameter* param = nullptr;

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    void sliderDragStarted (juce::Slider*) override  { repaint(); }
    void sliderDragEnded (juce::Slider*) override    { repaint(); }
};

/** Compact read-out box (tiny label + big serif value + optional dotted position bar).
    Drag up/down (or left/right) to change, wheel to nudge, double-click to type a value. */
class ValueBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    ValueBox (const juce::String& label);

    std::function<double()> get;
    std::function<void (double)> set;
    std::function<juce::String (double)> format;
    std::function<void()> onGestureStart, onGestureEnd;
    double minValue = 0.0, maxValue = 1.0, interval = 0.0, pixelsForFullRange = 200.0;
    bool showBar = true, editable = true, large = false;
    juce::String label;

    /** Binds the box to a host parameter (with proper begin/end gestures for automation). */
    void attachTo (juce::RangedAudioParameter& p, std::function<juce::String (double)> fmt = {});

    void refresh();          // repaint only if the shown value changed

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    double snap (double v) const;
    double dragStartValue = 0.0;
    juce::String lastText;
    bool dragging = false;
    std::unique_ptr<juce::TextEditor> editor;
};

/** Horizontal bar : caption on the left, filled bar, value on the right. Drag or wheel to change. */
class HBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit HBar (const juce::String& caption);
    std::function<double()> get;                 // 0..1
    std::function<void (double, bool first)> set;  // first = first change of the gesture (undo point)
    std::function<juce::String (double)> format;
    std::function<void()> onRelease;
    juce::Colour colour;
    juce::String caption;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::Rectangle<float> barArea() const;
    void setFromX (float x);
    bool first = true, dragging = false;
};

/** Macro fader with named zones (COMPLEXITY : CLEAN / LIGHT / CHOPPY / WILD / MAD).
    Centre = the style as is, left = less, right = more. Gentle detents on the zones ;
    click a zone name to jump there ; double-click = back to the centre. */
class ZoneFader : public juce::Component, public juce::SettableTooltipClient, private juce::AudioProcessorParameter::Listener, private juce::AsyncUpdater
{
public:
    ZoneFader (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& caption, juce::StringArray zones);
    ~ZoneFader() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    void parameterValueChanged (int, float) override { triggerAsyncUpdate(); }
    void parameterGestureChanged (int, bool) override {}
    void handleAsyncUpdate() override { repaint(); }
    juce::Rectangle<float> track() const;
    float valueForX (float x, bool detent) const;
    void setValue (float v);

    juce::RangedAudioParameter* param = nullptr;
    juce::String caption;
    juce::StringArray zones;
    bool dragging = false;
    float dragStartV = 0.0f;
};

/** Toggle button bound to a bool parameter (on = filled with the accent colour). */
struct ParamToggle : public juce::TextButton
{
    ParamToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& text);
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

/** Combo box bound to a choice parameter, with its caption drawn above. */
struct ParamChoice : public juce::Component
{
    ParamChoice (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& caption);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::ComboBox box;
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

} // namespace ui
