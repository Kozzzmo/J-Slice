#pragma once

#include "LookAndFeel.h"

class JSliceProcessor;

/** 8 bars x 16 steps + fill bar. Click = on/off, wheel = slice, drag up/down = velocity,
    shift+click = lock, alt+click = listen, right click = step menu. */
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (JSliceProcessor& p);

    void refresh();
    void tick();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::Rectangle<float> cellRect (int row, int col) const;
    int indexAt (juce::Point<int> p) const;     // 0..143 or -1
    const bc::Step& stepAt (int index) const;
    void showStepMenu (int index);

    JSliceProcessor& proc;
    bc::Pattern pattern;
    bc::Kit::Ptr kit;
    int playing = -1;
    int dragIndex = -1, dragStartVel = 100, dragStartY = 0;
    bool dragged = false;
    static constexpr int kRows = bc::kMaxBars + 1;
    static constexpr int kLabelW = 34, kHeaderH = 16;
};
