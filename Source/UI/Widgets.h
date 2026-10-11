#pragma once

#include "LookAndFeel.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{

/** Knob : dotted ring + pointer, mono label under it, big serif value under the label. */
struct Knob : public juce::Component, private juce::Slider::Listener
{
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& paramId, const juce::String& caption);
    ~Knob() override;
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::Slider slider;
    juce::String caption;
    std::function<juce::String (double)> format;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    juce::RangedAudioParameter* param = nullptr;

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
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
