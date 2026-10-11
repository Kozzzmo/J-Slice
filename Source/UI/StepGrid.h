#pragma once

#include "Widgets.h"

class JSliceProcessor;

/** 8 bars x 16 steps + fill bar.
      click            : add / remove a hit
      right click      : add a REVERSED hit (or flip reverse on an existing hit)
      drag left/right  : shorten / lengthen the hit
      drag up/down     : number of re-triggers (roll x1..x8)
      ctrl + drag      : velocity
      wheel            : change slice
      shift + click    : lock      alt + click : listen      shift + right click : step menu */
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (JSliceProcessor& p);

    void refresh();
    void tick();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum class DragMode { None, Pending, Gate, Roll, Velocity };

    juce::Rectangle<float> cellRect (int row, int col) const;
    int indexAt (juce::Point<int> p) const;     // 0..143 or -1
    const bc::Step& stepAt (int index) const;
    void showStepMenu (int index);
    void addStep (int index, bool reversed);

    JSliceProcessor& proc;
    ui::ParamToggle fillToggle;
    bc::Pattern pattern;
    bc::Kit::Ptr kit;
    int playing = -1;

    int dragIndex = -1;
    DragMode dragMode = DragMode::None;
    bc::Step dragStart;
    bool dragUndoPushed = false;
    juce::Point<int> downPos;

    static constexpr int kRows = bc::kMaxBars + 1;
    static constexpr int kLabelW = 56, kHeaderH = 17;
};
