#include "StepGrid.h"
#include "../PluginProcessor.h"

using namespace ui;

StepGrid::StepGrid (JSliceProcessor& p) : proc (p)
{
    setTooltip ("Click: add / remove hit   |   Wheel: change slice   |   Drag up/down: velocity\n"
                "Shift+click: lock (kept when you generate)   |   Alt+click: listen   |   Right click: options");
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
    if (s != playing) { playing = s; repaint(); }
}

juce::Rectangle<float> StepGrid::cellRect (int row, int col) const
{
    const float pitchX = (float) (getWidth() - kLabelW - 6 - 9) / 16.0f;          // 3 px gap between beats
    const float pitchY = (float) (getHeight() - kHeaderH - 6 - 6) / (float) kRows;  // 6 px gap before the fill row
    const float x = (float) kLabelW + col * pitchX + (float) (col / 4) * 3.0f;
    const float y = (float) kHeaderH + row * pitchY + (row == bc::kMaxBars ? 6.0f : 0.0f);
    return { x, y, pitchX - 2.0f, pitchY - 2.0f };
}

int StepGrid::indexAt (juce::Point<int> p) const
{
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < 16; ++c)
            if (cellRect (r, c).expanded (1.0f).contains (p.toFloat()))
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
    const auto ep = proc.currentEngineParams();

    g.setColour (col::panelEdge);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // header
    g.setFont (monoFont (10.0f));
    for (int c = 0; c < 16; c += 4)
    {
        auto r = cellRect (0, c);
        g.setColour (col::dim);
        g.drawText (String (c / 4 + 1), (int) r.getX(), 2, 20, kHeaderH - 2, Justification::left);
    }

    const int n = kit != nullptr ? (int) kit->slices.size() : 0;
    for (int r = 0; r < kRows; ++r)
    {
        const bool fillRow = r == bc::kMaxBars;
        const bool active = fillRow ? ep.fillEvery > 0 : r < ep.patternBars;
        auto first = cellRect (r, 0);
        g.setColour (fillRow ? col::accent.withAlpha (active ? 1.0f : 0.4f) : col::cream.withAlpha (active ? 0.9f : 0.3f));
        g.setFont (monoFont (11.0f));
        g.drawText (fillRow ? "FILL" : String (r + 1), 2, (int) first.getY(), kLabelW - 6, (int) first.getHeight(), Justification::centredRight);

        for (int c = 0; c < 16; ++c)
        {
            const int idx = fillRow ? bc::kMaxSteps + c : r * 16 + c;
            const auto& st = stepAt (idx);
            auto rc = cellRect (r, c);
            const float dimA = active ? 1.0f : 0.38f;

            g.setColour ((c % 4 == 0 ? Colour (0xff3d3a34) : Colour (0xff302e29)).withMultipliedAlpha (dimA));
            g.fillRoundedRectangle (rc, 3.0f);

            if (st.on && n > 0)
            {
                const int si = jlimit (0, n - 1, (int) st.slice);
                const auto cls = kit->slices[(size_t) si].cls;
                const float va = 0.4f + 0.6f * (float) st.vel / 127.0f;
                auto fillR = rc;
                if (st.gate < 0.999f) fillR = fillR.withWidth (jmax (6.0f, rc.getWidth() * (0.35f + 0.65f * st.gate)));
                g.setColour (classColour (cls).withAlpha (va * dimA));
                g.fillRoundedRectangle (fillR, 3.0f);

                g.setColour (col::body.withAlpha (dimA));
                g.setFont (monoFont (jmin (13.0f, rc.getHeight() * 0.6f)));
                g.drawText (String (si + 1), rc.toNearestInt(), Justification::centred);

                g.setFont (monoFont (8.5f));
                if (st.reverse)
                {
                    Path tri;
                    tri.addTriangle (rc.getX() + 2, rc.getY() + 5, rc.getX() + 7, rc.getY() + 2, rc.getX() + 7, rc.getY() + 8);
                    g.fillPath (tri);
                }
                for (int k = 1; k < st.roll; ++k)
                    g.fillRect (rc.getRight() - 4.0f * k - 1.0f, rc.getY() + 2.0f, 2.5f, 4.0f);
                if (st.pitch != 0)
                    g.drawText ((st.pitch > 0 ? "+" : "") + String ((int) st.pitch), rc.toNearestInt().reduced (2, 1), Justification::bottomRight);
            }

            if (st.locked)
            {
                g.setColour (col::accent);
                g.drawRoundedRectangle (rc.reduced (0.5f), 3.0f, 2.0f);
            }
            if (idx == playing)
            {
                g.setColour (Colours::white.withAlpha (0.9f));
                g.drawRoundedRectangle (rc.expanded (1.0f), 3.0f, 1.6f);
            }
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    dragged = false;
    dragIndex = indexAt (e.getPosition());
    if (dragIndex < 0 || kit == nullptr || kit->slices.empty()) { dragIndex = -1; return; }
    const auto& st = stepAt (dragIndex);

    if (e.mods.isPopupMenu()) { showStepMenu (dragIndex); dragIndex = -1; return; }
    if (e.mods.isAltDown())   { if (st.on) proc.previewSlice (st.slice); dragIndex = -1; return; }
    if (e.mods.isShiftDown())
    {
        proc.modifyStep (dragIndex, [] (bc::Step& s) { s.locked = ! s.locked; });
        dragIndex = -1;
        return;
    }
    dragStartVel = st.vel;
    dragStartY = e.y;
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIndex < 0 || ! stepAt (dragIndex).on) return;
    if (! dragged && std::abs (e.y - dragStartY) < 3) return;
    const int vel = juce::jlimit (1, 127, dragStartVel + (dragStartY - e.y));
    proc.modifyStep (dragIndex, [vel] (bc::Step& s) { s.vel = (uint8_t) vel; }, ! dragged);
    dragged = true;
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragIndex >= 0 && ! dragged)
    {
        const auto& st = stepAt (dragIndex);
        if (st.on)
        {
            if (! st.locked) proc.modifyStep (dragIndex, [] (bc::Step& s) { s = bc::Step(); });
        }
        else
        {
            const int abs = dragIndex >= bc::kMaxSteps ? (bc::kMaxSteps - 16 + dragIndex - bc::kMaxSteps) : dragIndex;
            const int sl = bc::Generator::naturalSliceForStep (*kit, abs);
            const auto cls = kit->slices[(size_t) juce::jmax (0, sl)].cls;
            proc.modifyStep (dragIndex, [sl, cls] (bc::Step& s)
            {
                s = bc::Step();
                s.on = true; s.slice = (int16_t) juce::jmax (0, sl); s.role = cls; s.vel = 110;
            });
            proc.previewSlice (juce::jmax (0, sl));
        }
    }
    dragIndex = -1;
    dragged = false;
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int idx = indexAt (e.getPosition());
    if (idx < 0 || kit == nullptr || kit->slices.empty() || ! stepAt (idx).on) return;
    const int dir = (w.deltaY > 0.0f) ? 1 : -1;
    const int n = (int) kit->slices.size();
    const int sl = (stepAt (idx).slice + dir + n) % n;
    const auto cls = kit->slices[(size_t) sl].cls;
    proc.modifyStep (idx, [sl, cls] (bc::Step& s) { s.slice = (int16_t) sl; s.role = cls; });
    proc.previewSlice (sl);
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
    for (int r = 1; r <= 4; ++r) roll.addItem (100 + r, r == 1 ? String ("Off") : "x" + String (r), true, st.roll == r);
    m.addSubMenu ("Roll", roll);
    PopupMenu pitch;
    for (int p = 12; p >= -12; --p) pitch.addItem (200 + p + 12, (p > 0 ? "+" : "") + String (p) + " st", true, st.pitch == p);
    m.addSubMenu ("Pitch", pitch);
    PopupMenu gate;
    for (int gIdx = 0; gIdx < 4; ++gIdx)
    {
        const float gv[] = { 1.0f, 0.75f, 0.5f, 0.25f };
        gate.addItem (300 + gIdx, String (roundToInt (gv[gIdx] * 100)) + " %", true, std::abs (st.gate - gv[gIdx]) < 0.05f);
    }
    m.addSubMenu ("Length", gate);
    PopupMenu vel;
    for (int v : { 127, 110, 90, 70, 50, 30 }) vel.addItem (400 + v, String (v), true, st.vel == v);
    m.addSubMenu ("Velocity", vel);
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
        auto ensureOn = [k, index] (bc::Step& s)
        {
            if (! s.on && k != nullptr && ! k->slices.empty())
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
        else if (r >= 400) { const int v = r - 400; pr.modifyStep (index, [ensureOn, v] (bc::Step& s) { ensureOn (s); s.vel = (uint8_t) v; }); }
        else if (r >= 300) { const float gv[] = { 1.0f, 0.75f, 0.5f, 0.25f }; const float g = gv[r - 300]; pr.modifyStep (index, [ensureOn, g] (bc::Step& s) { ensureOn (s); s.gate = g; }); }
        else if (r >= 200) { const int p = r - 200 - 12; pr.modifyStep (index, [ensureOn, p] (bc::Step& s) { ensureOn (s); s.pitch = (int8_t) p; }); }
        else if (r >= 100) { const int rl = r - 100; pr.modifyStep (index, [ensureOn, rl] (bc::Step& s) { ensureOn (s); s.roll = (uint8_t) rl; }); }
    });
}
