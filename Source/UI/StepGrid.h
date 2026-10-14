#pragma once

#include "Widgets.h"

class JSliceProcessor;

/** Shared step edits (used by the lanes and by the inspector). Index : 0..127 bars, 128..143 fill bar. */
namespace stepops
{
    const bc::Step& get (JSliceProcessor& p, int index);
    int absoluteStep (int index);                                           // fill steps map onto the last bar
    bool addHit (JSliceProcessor& p, int index, int slot, bool reversed);   // false when that break is not loaded
    void setSlice (JSliceProcessor& p, int index, int slice, bool undoable = true);
    void stepSlice (JSliceProcessor& p, int index, int dir);
    void switchBreak (JSliceProcessor& p, int index);
    void clear (JSliceProcessor& p, int index);
    void toggleFx (JSliceProcessor& p, int index, uint8_t bit);
    std::vector<float> envelope (const bc::Kit& kit, int slice, int points);  // 0..1 peak envelope of a slice
}

/** Two horizontal lanes : A on top, B under it. A step belongs to one lane only, so the two breaks interlock.
      click an empty cell     : add a hit from that lane's break (and select it)
      click a hit             : select + listen      drag left/right : length      drag up/down : re-triggers
      right click             : add a REVERSED hit (or flip reverse)
      double-click a hit      : delete               Delete key : delete the selected hit
      wheel / left-right keys : slice of the hit     up / down keys : velocity
    Under the lanes : step numbers, the velocity lane (click / paint) and the 8-bar map (click = go to that bar). */
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit StepGrid (JSliceProcessor& p);

    void refresh();
    void tick();

    // view
    void setZoom (int bars);                 // 1, 2 or 4 bars on screen
    int  getZoom() const                     { return zoom; }
    void page (int dir);
    void showFill (bool b);
    bool isShowingFill() const               { return fillPage; }
    void showBar (int bar);
    juce::String pageTitle() const;
    juce::String pageSuffix() const;
    std::function<void()> onViewChanged;
    std::function<void (int index)> onSelect;      // -1 = nothing selected
    std::function<void (int slot)> onNeedBreak;    // a lane was clicked but its break is not loaded

    int  getSelected() const                 { return selected; }
    int  getFirstIndex() const               { return firstIndex(); }
    int  getColumns() const                  { return columns(); }
    juce::Point<float> cellCentre (int lane, int col) const { return cellRect (lane, col).getCentre(); }
    juce::Point<float> veloPoint (int col, float vel01) const;
    juce::Point<float> mapPoint (int bar) const              { return mapBarRect (bar).getCentre(); }
    void select (int index);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override                  { repaint(); }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void focusLost (FocusChangeType) override { repaint(); }

private:
    enum class DragMode { None, Pending, Gate, Roll, Velocity };
    enum Zone { None = -1, LaneA = 0, LaneB = 1, Velo = 2, Map = 3 };

    struct Layout { float x0, w, laneH, laneBY, numY, veloY, veloH, mapY, mapH; };
    Layout layout() const;
    int columns() const                      { return fillPage ? bc::kStepsPerBar : zoom * bc::kStepsPerBar; }
    int firstIndex() const                   { return fillPage ? bc::kMaxSteps : firstBar * bc::kStepsPerBar; }
    juce::Rectangle<float> cellRect (int lane, int col) const;
    float colX (int col) const;
    float colW() const;
    int  colAt (float x) const;
    Zone zoneAt (juce::Point<float> p) const;
    int  patternBars() const;
    juce::Rectangle<float> mapBarRect (int bar) const;   // bar = kMaxBars for the fill box
    void rebuildEnvelopes();
    void setVelocityFromY (int index, float y, bool undoable);

    JSliceProcessor& proc;
    int playing = -1, selected = -1, hoverIndex = -1, hoverLane = -1;
    int zoom = 2, firstBar = 0;
    bool fillPage = false;

    std::array<bc::Kit::Ptr, bc::kNumBreaks> kits;
    std::array<std::vector<std::vector<float>>, bc::kNumBreaks> envs;

    int dragIndex = -1;
    DragMode dragMode = DragMode::None;
    bc::Step dragStart;
    bool dragUndoPushed = false, veloPainting = false;
    juce::Point<float> downPos;

    static constexpr float kLabelW = 46.0f;
};
