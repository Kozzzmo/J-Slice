#pragma once

#include "Widgets.h"
#include <set>

class JSliceProcessor;

/** 8 bars x 16 steps + fill bar.
      click            : add / remove a hit
      right click      : add a REVERSED hit (or flip reverse on an existing hit)
      drag left/right  : shorten / lengthen the hit
      drag up/down     : number of re-triggers (roll x1..x8)
      ctrl + drag      : velocity
      wheel            : change slice
      shift + click    : lock      alt + click : listen      shift + right click : step menu
      left / right arrow keys : previous / next slice on the selected step
    In an effect mode (Delay, Reverb, Lo-fi, Filter), click or paint across steps to add / remove that effect. */
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
    bool keyPressed (const juce::KeyPress&) override;
    void focusLost (FocusChangeType) override { repaint(); }

    /** 0 = notes, 1..kNumStepFx = edit that per-step effect. */
    void setEditMode (int m)   { editMode = juce::jlimit (0, bc::kNumStepFx, m); repaint(); }
    int getEditMode() const    { return editMode; }
    int getSelected() const    { return selected; }

private:
    enum class DragMode { None, Pending, Gate, Roll, Velocity };

    juce::Rectangle<float> cellRect (int row, int col) const;
    int indexAt (juce::Point<int> p) const;     // 0..143 or -1
    const bc::Step& stepAt (int index) const;
    void showStepMenu (int index);
    void addStep (int index, bool reversed, uint8_t fx = 0);
    void stepSlice (int index, int dir);
    void paintFx (int index, bool undoable);
    void ensureSendOn (uint8_t bit);

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
    int editMode = 0, selected = -1;
    bool fxPainting = false, fxTarget = true;
    std::set<int> fxPainted;

    static constexpr int kRows = bc::kMaxBars + 1;
    static constexpr int kLabelW = 56, kHeaderH = 17;
};
