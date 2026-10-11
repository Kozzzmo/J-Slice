#include "StepGrid.h"
#include "../PluginProcessor.h"

using namespace ui;

StepGrid::StepGrid (JSliceProcessor& p)
    : proc (p), fillToggle (p.apvts, PID::fillOn, "FILL")
{
    setTooltip ("Click: add / remove  |  right-click: add a REVERSED hit (or flip reverse)\n"
                "Drag left / right: shorter / longer  |  drag up / down: re-triggers (x1 to x8)\n"
                "Ctrl+drag: velocity  |  wheel or left / right arrows: slice of the selected step\n"
                "Shift+click: lock  |  alt+click: listen  |  shift+right-click: menu\n"
                "Effect modes (Delay, Reverb, Lo-fi, Filter): click or paint across steps to add / remove the effect");
    fillToggle.setTooltip ("Fill bar on / off (the FILL row replaces the last bar of every 2 / 4 / 8 bars)");
    fillToggle.onStateChange = [this] { repaint(); };
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    addAndMakeVisible (fillToggle);
    refresh();
}

void StepGrid::refresh()
{
    pattern = proc.getPattern();
    kit = proc.getKit();
    repaint();
}

void StepGrid::tick()
{
    const int s = proc.engine.uiStep.load();
    const int shown = proc.isRunning() ? s : -1;
    if (shown != playing) { playing = shown; repaint(); }
}

void StepGrid::resized()
{
    auto r = cellRect (bc::kMaxBars, 0);
    fillToggle.setBounds (2, (int) r.getY(), kLabelW - 8, (int) r.getHeight());
}

juce::Rectangle<float> StepGrid::cellRect (int row, int col) const
{
    const float pitchX = (float) (getWidth() - kLabelW - 2 - 9) / 16.0f;            // 3 px between beats
    const float pitchY = (float) (getHeight() - kHeaderH - 2 - 7) / (float) kRows;  // 7 px before the fill row
    const float x = (float) kLabelW + (float) col * pitchX + (float) (col / 4) * 3.0f;
    const float y = (float) kHeaderH + (float) row * pitchY + (row == bc::kMaxBars ? 7.0f : 0.0f);
    return { x, y, pitchX - 3.0f, pitchY - 3.0f };
}

int StepGrid::indexAt (juce::Point<int> p) const
{
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < 16; ++c)
            if (cellRect (r, c).expanded (1.5f).contains (p.toFloat()))
                return r == bc::kMaxBars ? bc::kMaxSteps + c : r * 16 + c;
    return -1;
}

const bc::Step& StepGrid::stepAt (int index) const
{
    return index >= bc::kMaxSteps ? pattern.fill[(size_t) (index - bc::kMaxSteps)] : pattern.steps[(size_t) index];
}

void StepGrid::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto& pl = pal();
    const auto ep = proc.currentEngineParams();
    const int len = ep.patternSteps;

    // beat numbers
    g.setFont (mono (11.0f));
    g.setColour (pl.dim);
    for (int c = 0; c < 16; c += 4)
        g.drawText (String (c / 4 + 1), (int) cellRect (0, c).getX() + 1, 0, 24, kHeaderH - 2, Justification::left);

    const int n = kit != nullptr ? (int) kit->slices.size() : 0;
    for (int r = 0; r < kRows; ++r)
    {
        const bool fillRow = r == bc::kMaxBars;
        if (! fillRow)
        {
            auto first = cellRect (r, 0);
            const bool rowUsed = r * 16 < len;
            g.setFont (serif (20.0f));
            g.setColour (rowUsed ? pl.text : pl.faint);
            g.drawText (String (r + 1), 4, (int) first.getY(), kLabelW - 14, (int) first.getHeight(), Justification::centredRight);
        }

        for (int c = 0; c < 16; ++c)
        {
            const int idx = fillRow ? bc::kMaxSteps + c : r * 16 + c;
            const bool inUse = fillRow ? ep.fillEvery > 0 : idx < len;
            const auto& st = stepAt (idx);
            auto rc = cellRect (r, c);
            const float a = inUse ? 1.0f : 0.3f;

            g.setColour (pl.raised.withMultipliedAlpha (a));
            g.fillRoundedRectangle (rc, 3.0f);
            g.setColour ((c % 4 == 0 ? pl.line : pl.faint).withMultipliedAlpha (a));
            g.drawRoundedRectangle (rc.reduced (0.5f), 3.0f, 1.0f);

            if (st.on && n > 0)
            {
                const int si = jlimit (0, n - 1, (int) st.slice);
                const auto cc = classColour (kit->slices[(size_t) si].cls);
                const float va = 0.35f + 0.65f * (float) st.vel / 127.0f;
                const float gw = jmax (5.0f, rc.getWidth() * jlimit (0.05f, 1.0f, st.gate));
                auto body = rc.reduced (1.0f).withWidth (gw - 2.0f);

                // body tint + top bar (the bar length = the hit length)
                g.setColour (cc.withAlpha (0.16f * a));
                g.fillRoundedRectangle (body, 2.5f);
                g.setColour (cc.withAlpha (va * a));
                g.fillRoundedRectangle (body.withHeight (jmin (5.0f, rc.getHeight() * 0.2f)), 1.5f);

                // re-triggers : the cell is divided
                const int roll = jlimit (1, bc::kMaxRoll, (int) st.roll);
                if (roll > 1)
                {
                    g.setColour (cc.withAlpha (0.7f * a));
                    const float y0 = rc.getY() + rc.getHeight() * 0.62f, y1 = rc.getBottom() - 2.0f;
                    for (int k = 0; k < roll; ++k)
                    {
                        const float x = rc.getX() + 2.0f + (rc.getWidth() - 4.0f) * (float) k / (float) roll;
                        g.fillRect (x, y0, jmax (1.0f, (rc.getWidth() - 4.0f) / (float) roll - 1.5f), y1 - y0);
                    }
                }

                g.setColour (pl.text.withMultipliedAlpha (a));
                if (roll > 1)
                {
                    g.setFont (serif (jmax (11.0f, rc.getHeight() * 0.55f)));
                    g.drawText (String (si + 1) + "  x" + String (roll), rc.withTrimmedTop (4.0f).withHeight (rc.getHeight() * 0.58f).toNearestInt(),
                                Justification::centred);
                }
                else
                {
                    g.setFont (serif (jmin (21.0f, rc.getHeight() * 0.72f)));
                    g.drawText (String (si + 1), rc.withTrimmedTop (3.0f).toNearestInt(), Justification::centred);
                }

                if (st.reverse)
                {
                    Path tri;
                    const float tx = rc.getX() + 3.0f, ty = rc.getBottom() - 4.0f;
                    tri.addTriangle (tx, ty - 3.0f, tx + 6.0f, ty - 6.5f, tx + 6.0f, ty + 0.5f);
                    g.setColour (pl.hot.withMultipliedAlpha (a));
                    g.fillPath (tri);
                }
                // per-step effect badges
                if (st.fx != 0)
                {
                    const Colour fxc[] = { pl.accent, pl.hat, pl.snare, pl.hot };
                    float bx = rc.getX() + (st.reverse ? 12.0f : 3.0f);
                    for (int b = 0; b < bc::kNumStepFx; ++b)
                    {
                        if ((st.fx & (1 << b)) == 0) continue;
                        g.setColour (fxc[b].withMultipliedAlpha (a));
                        g.fillRoundedRectangle (bx, rc.getBottom() - 7.0f, 5.0f, 5.0f, 1.0f);
                        bx += 7.0f;
                    }
                }
                if (st.pitch != 0)
                {
                    g.setColour (pl.dim);
                    g.setFont (mono (10.0f, true, 0.0f));
                    g.drawText ((st.pitch > 0 ? "+" : "") + String ((int) st.pitch), rc.toNearestInt().reduced (2, 1), Justification::bottomRight);
                }
            }

            if (editMode > 0)
            {
                const uint8_t bit = (uint8_t) (1 << (editMode - 1));
                if (st.on && (st.fx & bit) != 0)
                {
                    g.setColour (pl.accent.withAlpha (0.28f * a));
                    g.fillRoundedRectangle (rc, 3.0f);
                    g.setColour (pl.accent.withMultipliedAlpha (a));
                    g.drawRoundedRectangle (rc.reduced (0.5f), 3.0f, 1.6f);
                }
                else if (st.on)
                {
                    g.setColour (pl.bg.withAlpha (0.35f));
                    g.fillRoundedRectangle (rc, 3.0f);
                }
            }
            if (idx == selected && hasKeyboardFocus (true))
            {
                g.setColour (pl.active);
                g.drawRoundedRectangle (rc.expanded (2.0f), 4.0f, 1.4f);
            }
            if (st.locked)
            {
                g.setColour (pl.active);
                g.drawRoundedRectangle (rc.reduced (0.5f), 3.0f, 1.8f);
            }
            if (idx == playing)
            {
                g.setColour (pl.text);
                g.drawRoundedRectangle (rc.expanded (1.0f), 3.5f, 1.6f);
            }
        }
    }
}

void StepGrid::paintOverChildren (juce::Graphics& g)
{
    // live read-out while dragging
    if (dragIndex < 0 || (dragMode != DragMode::Gate && dragMode != DragMode::Roll && dragMode != DragMode::Velocity)) return;
    const auto& st = stepAt (dragIndex);
    juce::String txt = dragMode == DragMode::Gate ? (st.gate >= 0.999f ? juce::String ("LEN FULL") : "LEN " + juce::String (juce::roundToInt (st.gate * 100)) + "%")
                     : dragMode == DragMode::Roll ? "RETRIG x" + juce::String ((int) st.roll)
                                                  : "VEL " + juce::String ((int) st.vel);
    const int row = dragIndex >= bc::kMaxSteps ? bc::kMaxBars : dragIndex / 16;
    const int col = dragIndex >= bc::kMaxSteps ? dragIndex - bc::kMaxSteps : dragIndex % 16;
    auto rc = cellRect (row, col);
    auto box = juce::Rectangle<float> (118.0f, 24.0f).withCentre ({ rc.getCentreX(), rc.getY() - 13.0f });
    if (box.getY() < 0.0f) box.setY (rc.getBottom() + 3.0f);
    box = box.constrainedWithin (getLocalBounds().toFloat());
    g.setColour (ui::pal().accent);
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (ui::pal().onAccent);
    g.setFont (ui::mono (12.5f));
    g.drawText (txt, box.toNearestInt(), juce::Justification::centred);
}

//==============================================================================
void StepGrid::addStep (int index, bool reversed, uint8_t fx)
{
    if (kit == nullptr || kit->slices.empty()) return;
    const int abs = index >= bc::kMaxSteps ? (bc::kMaxSteps - 16 + index - bc::kMaxSteps) : index;
    const int sl = juce::jmax (0, bc::Generator::naturalSliceForStep (*kit, abs));
    const auto cls = kit->slices[(size_t) sl].cls;
    proc.modifyStep (index, [sl, cls, reversed] (bc::Step& s)
    {
        s = bc::Step();
        s.on = true; s.slice = (int16_t) sl; s.role = cls; s.vel = 110; s.reverse = reversed;
    }, true);
    if (fx != 0) proc.modifyStep (index, [fx] (bc::Step& s) { s.fx = fx; }, false);
    proc.previewSlice (sl);
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    dragMode = DragMode::None;
    dragUndoPushed = false;
    dragIndex = indexAt (e.getPosition());
    downPos = e.getPosition();
    if (dragIndex < 0 || kit == nullptr || kit->slices.empty()) { dragIndex = -1; return; }
    const auto st = stepAt (dragIndex);
    selected = dragIndex;
    repaint();

    if (editMode > 0 && ! e.mods.isPopupMenu() && ! e.mods.isShiftDown() && ! e.mods.isAltDown())
    {
        const uint8_t bit = (uint8_t) (1 << (editMode - 1));
        fxTarget = ! (st.on && (st.fx & bit) != 0);
        fxPainting = true;
        fxPainted.clear();
        paintFx (dragIndex, true);
        dragMode = DragMode::None;
        return;
    }

    if (e.mods.isPopupMenu())
    {
        if (e.mods.isShiftDown()) showStepMenu (dragIndex);
        else if (st.on)           proc.modifyStep (dragIndex, [] (bc::Step& s) { s.reverse = ! s.reverse; });
        else                      addStep (dragIndex, true);
        dragIndex = -1;
        return;
    }
    if (e.mods.isMiddleButtonDown()) { showStepMenu (dragIndex); dragIndex = -1; return; }
    if (e.mods.isAltDown())   { if (st.on) proc.previewSlice (st.slice); dragIndex = -1; return; }
    if (e.mods.isShiftDown())
    {
        proc.modifyStep (dragIndex, [] (bc::Step& s) { s.locked = ! s.locked; });
        dragIndex = -1;
        return;
    }
    dragStart = st;
    dragMode = DragMode::Pending;
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (fxPainting)
    {
        const int idx = indexAt (e.getPosition());
        if (idx >= 0) paintFx (idx, false);
        return;
    }
    if (dragIndex < 0 || ! dragStart.on) return;
    const int dx = e.x - downPos.x, dy = e.y - downPos.y;

    if (dragMode == DragMode::Pending)
    {
        if (std::abs (dx) < 5 && std::abs (dy) < 5) return;
        if (e.mods.isCommandDown() || e.mods.isCtrlDown()) dragMode = DragMode::Velocity;
        else dragMode = std::abs (dx) > std::abs (dy) ? DragMode::Gate : DragMode::Roll;
        setMouseCursor (dragMode == DragMode::Gate ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
    }

    const float cellW = cellRect (0, 0).getWidth();
    std::function<void (bc::Step&)> fn;
    if (dragMode == DragMode::Gate)
    {
        const float gate = juce::jlimit (0.05f, 1.0f, dragStart.gate + (float) dx / (cellW * 2.5f));
        const float snapped = gate > 0.97f ? 1.0f : gate;
        fn = [snapped] (bc::Step& s) { s.gate = snapped; };
    }
    else if (dragMode == DragMode::Roll)
    {
        const int roll = juce::jlimit (1, bc::kMaxRoll, (int) dragStart.roll + (int) std::round ((float) -dy / 14.0f));
        fn = [roll] (bc::Step& s) { s.roll = (uint8_t) roll; };
    }
    else if (dragMode == DragMode::Velocity)
    {
        const int vel = juce::jlimit (1, 127, (int) dragStart.vel - dy);
        fn = [vel] (bc::Step& s) { s.vel = (uint8_t) vel; };
    }
    if (fn)
    {
        proc.modifyStep (dragIndex, fn, ! dragUndoPushed);
        dragUndoPushed = true;
    }
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    if (fxPainting)
    {
        fxPainting = false;
        fxPainted.clear();
        dragIndex = -1;
        return;
    }
    if (dragIndex >= 0 && dragMode == DragMode::Pending)
    {
        const auto& st = stepAt (dragIndex);
        if (st.on) { if (! st.locked) proc.modifyStep (dragIndex, [] (bc::Step& s) { s = bc::Step(); }); }
        else addStep (dragIndex, false);
    }
    else if (dragIndex >= 0 && dragMode == DragMode::Roll)
    {
        proc.previewSlice (stepAt (dragIndex).slice);
    }
    dragIndex = -1;
    dragMode = DragMode::None;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int idx = indexAt (e.getPosition());
    if (idx < 0) return;
    selected = idx;
    stepSlice (idx, w.deltaY > 0.0f ? 1 : -1);
}

void StepGrid::ensureSendOn (uint8_t bit)
{
    // marking a step for the delay / reverb switches that effect on, otherwise nothing would be heard
    const char* id = bit == bc::FxDelay ? PID::dlyOn : (bit == bc::FxReverb ? PID::revOn : nullptr);
    if (id == nullptr) return;
    if (auto* p = proc.apvts.getParameter (id))
        if (p->getValue() < 0.5f) p->setValueNotifyingHost (1.0f);
}

void StepGrid::paintFx (int index, bool undoable)
{
    if (fxPainted.count (index) > 0 || editMode <= 0) return;
    fxPainted.insert (index);
    const uint8_t bit = (uint8_t) (1 << (editMode - 1));
    const auto& st = stepAt (index);
    if (! st.on)
    {
        if (fxTarget && undoable) addStep (index, false, bit);   // only the first click creates hits
        if (fxTarget) ensureSendOn (bit);
        return;
    }
    const bool target = fxTarget;
    proc.modifyStep (index, [bit, target] (bc::Step& s) { s.fx = (uint8_t) (target ? (s.fx | bit) : (s.fx & ~bit)); }, undoable);
    if (target) ensureSendOn (bit);
}

void StepGrid::stepSlice (int index, int dir)
{
    if (index < 0 || kit == nullptr || kit->slices.empty() || ! stepAt (index).on) return;
    const int n = (int) kit->slices.size();
    const int sl = (stepAt (index).slice + dir + n) % n;
    const auto cls = kit->slices[(size_t) sl].cls;
    proc.modifyStep (index, [sl, cls] (bc::Step& s) { s.slice = (int16_t) sl; s.role = cls; });
    proc.previewSlice (sl);
}

bool StepGrid::keyPressed (const juce::KeyPress& k)
{
    if (selected < 0) return false;
    if (k.getKeyCode() == juce::KeyPress::leftKey)  { stepSlice (selected, -1); return true; }
    if (k.getKeyCode() == juce::KeyPress::rightKey) { stepSlice (selected, 1);  return true; }
    if (k.getKeyCode() == juce::KeyPress::escapeKey) { selected = -1; repaint(); return true; }
    return false;
}

void StepGrid::showStepMenu (int index)
{
    using namespace juce;
    const auto st = stepAt (index);
    const int n = kit != nullptr ? (int) kit->slices.size() : 0;
    PopupMenu m;
    m.addSectionHeader (index >= bc::kMaxSteps ? "Fill, step " + String (index - bc::kMaxSteps + 1)
                                               : "Bar " + String (index / 16 + 1) + ", step " + String (index % 16 + 1));
    PopupMenu slices;
    for (int i = 0; i < n; ++i)
        slices.addItem (1000 + i, String (i + 1) + "  " + bc::hitClassName (kit->slices[(size_t) i].cls), true, st.on && st.slice == i);
    m.addSubMenu ("Slice", slices);
    m.addItem (1, "Reverse", true, st.reverse);
    PopupMenu roll;
    for (int r = 1; r <= bc::kMaxRoll; ++r) roll.addItem (100 + r, r == 1 ? String ("Off") : "x" + String (r), true, st.roll == r);
    m.addSubMenu ("Re-triggers", roll);
    PopupMenu pitch;
    for (int p = 12; p >= -12; --p) pitch.addItem (200 + p + 12, (p > 0 ? "+" : "") + String (p) + " st", true, st.pitch == p);
    m.addSubMenu ("Pitch", pitch);
    PopupMenu gate;
    const float gv[] = { 1.0f, 0.75f, 0.5f, 0.25f, 0.1f };
    for (int gIdx = 0; gIdx < 5; ++gIdx)
        gate.addItem (300 + gIdx, gIdx == 0 ? String ("Full") : String (roundToInt (gv[gIdx] * 100)) + " %", true, std::abs (st.gate - gv[gIdx]) < 0.03f);
    m.addSubMenu ("Length", gate);
    PopupMenu vel;
    for (int v : { 127, 110, 90, 70, 50, 30 }) vel.addItem (400 + v, String (v), true, st.vel == v);
    m.addSubMenu ("Velocity", vel);
    PopupMenu fx;
    for (int b = 0; b < bc::kNumStepFx; ++b) fx.addItem (500 + b, bc::stepFxName (b), true, (st.fx & (1 << b)) != 0);
    m.addSubMenu ("Effects", fx);
    m.addSeparator();
    m.addItem (2, "Lock", true, st.locked);
    m.addItem (3, "Listen", st.on);
    m.addItem (4, "Clear step", st.on);

    SafePointer<StepGrid> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, index, st] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        auto& pr = self.proc;
        auto k = self.kit;
        if (k == nullptr || k->slices.empty()) return;
        auto ensureOn = [k, index] (bc::Step& s)
        {
            if (! s.on)
            {
                const int abs = index >= bc::kMaxSteps ? (bc::kMaxSteps - 16 + index - bc::kMaxSteps) : index;
                const int sl = juce::jmax (0, bc::Generator::naturalSliceForStep (*k, abs));
                s.on = true; s.slice = (int16_t) sl; s.role = k->slices[(size_t) sl].cls; s.vel = 110;
            }
        };
        if (r >= 1000)
        {
            const int sl = r - 1000;
            const auto cls = k->slices[(size_t) sl].cls;
            pr.modifyStep (index, [sl, cls] (bc::Step& s) { if (! s.on) { s.on = true; s.vel = 110; } s.slice = (int16_t) sl; s.role = cls; });
            pr.previewSlice (sl);
        }
        else if (r == 1)  pr.modifyStep (index, [ensureOn] (bc::Step& s) { ensureOn (s); s.reverse = ! s.reverse; });
        else if (r == 2)  pr.modifyStep (index, [] (bc::Step& s) { s.locked = ! s.locked; });
        else if (r == 3)  pr.previewSlice (st.slice);
        else if (r == 4)  pr.modifyStep (index, [] (bc::Step& s) { s = bc::Step(); });
        else if (r >= 500)
        {
            const uint8_t bit = (uint8_t) (1 << (r - 500));
            pr.modifyStep (index, [ensureOn, bit] (bc::Step& s) { ensureOn (s); s.fx ^= bit; });
            self.ensureSendOn (bit);
        }
        else if (r >= 400) { const int v = r - 400; pr.modifyStep (index, [ensureOn, v] (bc::Step& s) { ensureOn (s); s.vel = (uint8_t) v; }); }
        else if (r >= 300) { const float gvals[] = { 1.0f, 0.75f, 0.5f, 0.25f, 0.1f }; const float gt = gvals[r - 300]; pr.modifyStep (index, [ensureOn, gt] (bc::Step& s) { ensureOn (s); s.gate = gt; }); }
        else if (r >= 200) { const int p = r - 200 - 12; pr.modifyStep (index, [ensureOn, p] (bc::Step& s) { ensureOn (s); s.pitch = (int8_t) p; }); }
        else if (r >= 100) { const int rl = r - 100; pr.modifyStep (index, [ensureOn, rl] (bc::Step& s) { ensureOn (s); s.roll = (uint8_t) rl; }); }
    });
}
