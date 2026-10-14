#include "StepGrid.h"
#include "../PluginProcessor.h"

using namespace ui;

//==============================================================================
namespace stepops
{
    const bc::Step& get (JSliceProcessor& p, int index)
    {
        static const bc::Step empty;
        const auto& pat = p.getPattern();
        if (index >= 0 && index < bc::kMaxSteps) return pat.steps[(size_t) index];
        if (index >= bc::kMaxSteps && index < bc::kMaxSteps + bc::kStepsPerBar) return pat.fill[(size_t) (index - bc::kMaxSteps)];
        return empty;
    }

    int absoluteStep (int index)
    {
        return index >= bc::kMaxSteps ? bc::kMaxSteps - bc::kStepsPerBar + (index - bc::kMaxSteps) : index;
    }

    bool addHit (JSliceProcessor& p, int index, int slot, bool reversed)
    {
        auto k = p.getKit (slot);
        if (k == nullptr || k->slices.empty()) return false;
        if (get (p, index).locked) return true;
        const int sl = juce::jmax (0, bc::Generator::naturalSliceForStep (*k, absoluteStep (index)));
        const auto cls = k->slices[(size_t) sl].cls;
        p.modifyStep (index, [sl, cls, reversed, slot] (bc::Step& s)
        {
            s = bc::Step();
            s.on = true; s.slice = (int16_t) sl; s.role = cls; s.vel = 110; s.reverse = reversed; s.src = (uint8_t) slot;
        }, true);
        p.previewSlice (slot, sl);
        return true;
    }

    void setSlice (JSliceProcessor& p, int index, int slice, bool undoable)
    {
        const auto& st = get (p, index);
        auto k = p.getKit (st.src);
        if (! st.on || k == nullptr || k->slices.empty()) return;
        const int sl = juce::jlimit (0, (int) k->slices.size() - 1, slice);
        const auto cls = k->slices[(size_t) sl].cls;
        p.modifyStep (index, [sl, cls] (bc::Step& s) { s.slice = (int16_t) sl; s.role = cls; }, undoable);
        p.previewSlice (st.src, sl);
    }

    void stepSlice (JSliceProcessor& p, int index, int dir)
    {
        const auto& st = get (p, index);
        auto k = p.getKit (st.src);
        if (! st.on || k == nullptr || k->slices.empty()) return;
        const int n = (int) k->slices.size();
        setSlice (p, index, (st.slice + dir + n) % n);
    }

    void switchBreak (JSliceProcessor& p, int index)
    {
        const auto& st = get (p, index);
        if (! st.on) return;
        const int other = 1 - st.src;
        auto k = p.getKit (other);
        if (k == nullptr || k->slices.empty()) return;
        // keep the same kind of drum when possible
        int sl = -1;
        for (int i = 0; i < (int) k->slices.size() && sl < 0; ++i)
            if (k->slices[(size_t) i].cls == st.role) sl = i;
        if (sl < 0) sl = juce::jmax (0, bc::Generator::naturalSliceForStep (*k, absoluteStep (index)));
        const auto cls = k->slices[(size_t) sl].cls;
        p.modifyStep (index, [sl, cls, other] (bc::Step& s) { s.src = (uint8_t) other; s.slice = (int16_t) sl; s.role = cls; });
        p.previewSlice (other, sl);
    }

    void clear (JSliceProcessor& p, int index)
    {
        if (get (p, index).locked) return;
        p.modifyStep (index, [] (bc::Step& s) { s = bc::Step(); });
    }

    void toggleFx (JSliceProcessor& p, int index, uint8_t bit)
    {
        const auto& st = get (p, index);
        if (! st.on) return;
        const bool adding = (st.fx & bit) == 0;
        p.modifyStep (index, [bit] (bc::Step& s) { s.fx = (uint8_t) (s.fx ^ bit); });
        // marking a step for the delay / reverb switches that effect on, otherwise nothing would be heard,
        // and makes it play on the marked steps only (not on every hit, not on every snare)
        const char* id = bit == bc::FxDelay ? PID::dlyOn : (bit == bc::FxReverb ? PID::revOn : nullptr);
        const char* src = bit == bc::FxDelay ? PID::dlySrc : (bit == bc::FxReverb ? PID::revSrc : nullptr);
        if (adding && id != nullptr)
        {
            if (auto* prm = p.apvts.getParameter (id))
                if (prm->getValue() < 0.5f) prm->setValueNotifyingHost (1.0f);
            if (auto* prm = p.apvts.getParameter (src))
                if (prm->getValue() < 0.99f) prm->setValueNotifyingHost (1.0f);   // last choice = "Marked only"
        }
    }

    std::vector<float> envelope (const bc::Kit& kit, int slice, int points)
    {
        std::vector<float> env ((size_t) juce::jmax (1, points), 0.0f);
        if (kit.audio == nullptr || slice < 0 || slice >= (int) kit.slices.size()) return env;
        const auto& sl = kit.slices[(size_t) slice];
        const auto& b = kit.audio->buffer;
        const int64_t s0 = juce::jlimit ((int64_t) 0, (int64_t) b.getNumSamples(), sl.start);
        const int64_t s1 = juce::jlimit (s0, (int64_t) b.getNumSamples(), sl.end);
        const int64_t len = juce::jmax ((int64_t) 1, s1 - s0);
        float peak = 1.0e-4f;
        for (int i = 0; i < points; ++i)
        {
            const int64_t a = s0 + len * i / points, e = juce::jmax (a + 1, s0 + len * (i + 1) / points);
            float m = 0.0f;
            for (int64_t k = a; k < e && k < s1; k += 2)
                m = juce::jmax (m, std::abs (b.getSample (0, (int) k)), std::abs (b.getSample (1, (int) k)));
            env[(size_t) i] = m;
            peak = juce::jmax (peak, m);
        }
        for (auto& v : env) v /= peak;
        return env;
    }
}

//==============================================================================
StepGrid::StepGrid (JSliceProcessor& p) : proc (p)
{
    setTooltip ("Click an empty cell : add a hit from that break  |  click a hit : select and listen\n"
                "Drag a hit left / right : length  |  up / down : re-triggers  |  double-click : delete\n"
                "Right-click : reversed hit  |  wheel or left / right keys : slice  |  up / down keys : velocity");
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    refresh();
}

void StepGrid::refresh()
{
    bool changed = false;
    for (int s = 0; s < bc::kNumBreaks; ++s)
    {
        auto k = proc.getKit (s);
        if (k != kits[(size_t) s]) { kits[(size_t) s] = k; changed = true; }
    }
    if (changed) rebuildEnvelopes();
    if (selected >= 0 && ! stepops::get (proc, selected).on) select (-1);
    if (fillPage == false && firstBar >= patternBars()) { firstBar = 0; if (onViewChanged) onViewChanged(); }
    repaint();
}

void StepGrid::rebuildEnvelopes()
{
    for (int s = 0; s < bc::kNumBreaks; ++s)
    {
        auto& e = envs[(size_t) s];
        e.clear();
        if (auto k = kits[(size_t) s])
            for (int i = 0; i < (int) k->slices.size(); ++i)
                e.push_back (stepops::envelope (*k, i, 16));
    }
}

void StepGrid::tick()
{
    const int s = proc.engine.uiStep.load();
    const int shown = proc.isRunning() ? s : -1;
    if (shown != playing) { playing = shown; repaint(); }
}

int StepGrid::patternBars() const
{
    const int steps = (int) proc.apvts.getRawParameterValue (PID::steps)->load();
    return juce::jlimit (1, bc::kMaxBars, (steps + bc::kStepsPerBar - 1) / bc::kStepsPerBar);
}

//==============================================================================
void StepGrid::setZoom (int bars)
{
    zoom = bars >= 4 ? 4 : (bars >= 2 ? 2 : 1);
    firstBar = (firstBar / zoom) * zoom;
    fillPage = false;
    repaint();
    if (onViewChanged) onViewChanged();
}

void StepGrid::page (int dir)
{
    const int bars = patternBars();
    if (fillPage)
    {
        fillPage = false;
        firstBar = dir < 0 ? ((bars - 1) / zoom) * zoom : 0;
    }
    else
    {
        const int next = firstBar + dir * zoom;
        if (next >= bars || next >= bc::kMaxBars) { fillPage = true; }
        else if (next < 0) { fillPage = true; }
        else firstBar = next;
    }
    repaint();
    if (onViewChanged) onViewChanged();
}

void StepGrid::showFill (bool b)
{
    fillPage = b;
    repaint();
    if (onViewChanged) onViewChanged();
}

void StepGrid::showBar (int bar)
{
    fillPage = false;
    firstBar = (juce::jlimit (0, bc::kMaxBars - 1, bar) / zoom) * zoom;
    repaint();
    if (onViewChanged) onViewChanged();
}

juce::String StepGrid::pageTitle() const
{
    if (fillPage) return "Fill bar";
    const int last = juce::jmin (bc::kMaxBars, firstBar + zoom);
    return zoom == 1 ? "Bar " + juce::String (firstBar + 1)
                     : "Bars " + juce::String (firstBar + 1) + juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")) + juce::String (last);
}

juce::String StepGrid::pageSuffix() const
{
    return fillPage ? juce::String() : "/ " + juce::String (patternBars());
}

void StepGrid::select (int index)
{
    if (index == selected) return;
    selected = index;
    repaint();
    if (onSelect) onSelect (selected);
}

//==============================================================================
StepGrid::Layout StepGrid::layout() const
{
    Layout l;
    const float h = (float) getHeight();
    l.x0 = kLabelW;
    l.w = (float) getWidth() - kLabelW;
    l.mapH = 30.0f;
    l.veloH = 42.0f;
    const float numH = 20.0f, gap = 6.0f;
    l.laneH = std::floor ((h - l.mapH - l.veloH - numH - gap * 3.0f) / 2.0f);
    l.laneBY = l.laneH + gap;
    l.numY = l.laneBY + l.laneH;
    l.veloY = l.numY + numH;
    l.mapY = l.veloY + l.veloH + gap * 2.0f;
    return l;
}

float StepGrid::colW() const
{
    const auto l = layout();
    const int cols = columns();
    const float gaps = (float) (cols / 4 - 1) * 3.0f + (float) (cols / 16 - 1) * 6.0f;
    return (l.w - gaps) / (float) cols;
}

float StepGrid::colX (int col) const
{
    return kLabelW + (float) col * colW() + (float) (col / 4) * 3.0f + (float) (col / 16) * 6.0f;
}

int StepGrid::colAt (float x) const
{
    const int cols = columns();
    for (int c = 0; c < cols; ++c)
        if (x < colX (c) + colW() + 1.5f) return x >= colX (c) - 2.0f ? c : (c > 0 ? c - 1 : 0);
    return cols - 1;
}

juce::Rectangle<float> StepGrid::cellRect (int lane, int col) const
{
    const auto l = layout();
    return { colX (col), lane == 0 ? 0.0f : l.laneBY, colW() - 2.0f, l.laneH };
}

StepGrid::Zone StepGrid::zoneAt (juce::Point<float> p) const
{
    const auto l = layout();
    if (p.y >= l.mapY - 3.0f) return Map;
    if (p.x < kLabelW - 2.0f) return None;
    if (p.y < l.laneH + 3.0f) return LaneA;
    if (p.y >= l.laneBY - 3.0f && p.y < l.numY) return LaneB;
    if (p.y >= l.veloY - 2.0f && p.y < l.veloY + l.veloH + 4.0f) return Velo;
    return None;
}

juce::Rectangle<float> StepGrid::mapBarRect (int bar) const
{
    const auto l = layout();
    const float fillW = 40.0f, gap = 4.0f;
    const float bw = (l.w - fillW - gap * (float) bc::kMaxBars) / (float) bc::kMaxBars;
    const float x = l.x0 + (float) bar * (bw + gap);
    return { x, l.mapY, bar == bc::kMaxBars ? fillW : bw, l.mapH };
}

juce::Point<float> StepGrid::veloPoint (int col, float vel01) const
{
    const auto l = layout();
    return { colX (col) + colW() * 0.5f, l.veloY + 6.0f + (l.veloH - 10.0f) * (1.0f - vel01) };
}

//==============================================================================
void StepGrid::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto& pl = pal();
    const auto l = layout();
    const auto& pat = proc.getPattern();
    const auto ep = proc.currentEngineParams();
    const int len = ep.patternSteps;
    const int cols = columns(), first = firstIndex();
    const bool bActive = proc.isBreakBActive();
    const float cw = colW();

    // ---- lane labels
    for (int lane = 0; lane < 2; ++lane)
    {
        Rectangle<float> lr (0.0f, lane == 0 ? 0.0f : l.laneBY, kLabelW - 6.0f, l.laneH);
        const auto laneCol = breakColour (lane);
        const bool loaded = kits[(size_t) lane] != nullptr;
        const bool on = lane == 0 || bActive;
        g.setColour (on && loaded ? laneCol : pl.raised);
        g.fillRoundedRectangle (lr, 5.0f);
        g.setColour (on && loaded ? laneCol : pl.line);
        g.drawRoundedRectangle (lr.reduced (0.5f), 5.0f, 1.0f);
        g.setFont (serif (34.0f));
        g.setColour (on && loaded ? pl.onAccent : (loaded ? laneCol.withAlpha (0.6f) : pl.dim));
        g.drawText (lane == 0 ? "A" : "B", lr.toNearestInt(), Justification::centred);
    }
    g.setFont (mono (10.5f));
    g.setColour (pl.dim);
    g.drawText ("VELO", 0, (int) l.veloY, (int) kLabelW, (int) l.veloH, Justification::centredLeft);
    g.drawText ("MAP", 0, (int) l.mapY, (int) kLabelW, (int) l.mapH, Justification::centredLeft);

    // ---- lane backgrounds
    for (int lane = 0; lane < 2; ++lane)
    {
        Rectangle<float> bg (l.x0 - 3.0f, lane == 0 ? 0.0f : l.laneBY, l.w + 3.0f, l.laneH);
        g.setColour (pl.inset);
        g.fillRoundedRectangle (bg, 5.0f);
        g.setColour (pl.line);
        g.drawRoundedRectangle (bg.reduced (0.5f), 5.0f, 1.0f);
    }

    // ---- cells
    for (int c = 0; c < cols; ++c)
    {
        const int idx = first + c;
        const bool inUse = fillPage ? ep.fillEvery > 0 : idx < len;
        const auto& st = idx >= bc::kMaxSteps ? pat.fill[(size_t) (idx - bc::kMaxSteps)] : pat.steps[(size_t) idx];
        const float alpha = inUse ? 1.0f : 0.3f;

        if (idx == playing)
        {
            g.setColour (pl.text.withAlpha (0.10f));
            g.fillRoundedRectangle (Rectangle<float> (colX (c) - 1.0f, 0.0f, cw, l.numY + 14.0f), 3.0f);
        }

        for (int lane = 0; lane < 2; ++lane)
        {
            auto rc = cellRect (lane, c).reduced (0.0f, 4.0f);
            const auto bcol = breakColour (lane);
            const bool mine = st.on && st.src == lane;

            // empty slot : small beat-coloured dots
            if (! mine)
            {
                const bool beat = c % 4 == 0;
                g.setColour ((beat ? pl.line : pl.faint).withMultipliedAlpha (alpha));
                g.fillRoundedRectangle (Rectangle<float> (4.0f, 4.0f).withCentre (rc.getCentre()), 1.0f);
                if (idx == hoverIndex && lane == hoverLane && kits[(size_t) lane] != nullptr)
                {
                    g.setColour (bcol.withAlpha (0.16f));
                    g.fillRoundedRectangle (rc, 4.0f);
                    g.setColour (bcol.withAlpha (0.5f));
                    g.drawRoundedRectangle (rc.reduced (0.5f), 4.0f, 1.0f);
                }
                continue;
            }

            const bool muted = lane == 1 && ! bActive;
            const float a = alpha * (muted ? 0.35f : 1.0f);
            const float va = 0.45f + 0.55f * (float) st.vel / 127.0f;
            auto tile = rc.withWidth (jmax (6.0f, rc.getWidth() * jlimit (0.05f, 1.0f, st.gate)));

            g.setColour (bcol.withAlpha (0.20f * a * va));
            g.fillRoundedRectangle (tile, 4.0f);
            g.setColour (bcol.withAlpha (0.95f * a));
            g.drawRoundedRectangle (tile.reduced (0.6f), 4.0f, 1.3f);

            // slice glyph : its own envelope (mirrored when reversed)
            const auto& env = envs[(size_t) lane];
            if (st.slice >= 0 && st.slice < (int) env.size() && tile.getWidth() > 5.0f)
            {
                const auto& e = env[(size_t) st.slice];
                auto ga = tile.reduced (tile.getWidth() > 12.0f ? 3.0f : 1.5f, 6.0f).withTrimmedBottom (rc.getWidth() >= 11.0f ? rc.getHeight() * 0.36f : 0.0f);
                const float mid = ga.getCentreY();
                Path p;
                p.startNewSubPath (ga.getX(), mid);
                const int n = (int) e.size();
                for (int i = 0; i < n; ++i)
                {
                    const float v = e[(size_t) (st.reverse ? n - 1 - i : i)];
                    p.lineTo (ga.getX() + ga.getWidth() * (float) i / (float) (n - 1), mid - v * ga.getHeight() * 0.5f);
                }
                for (int i = n - 1; i >= 0; --i)
                {
                    const float v = e[(size_t) (st.reverse ? n - 1 - i : i)];
                    p.lineTo (ga.getX() + ga.getWidth() * (float) i / (float) (n - 1), mid + v * ga.getHeight() * 0.5f);
                }
                p.closeSubPath();
                g.setColour (bcol.withAlpha (0.9f * a));
                g.fillPath (p);
            }

            // slice number
            if (rc.getWidth() >= 11.0f)
            {
                g.setFont (serif (jlimit (12.0f, 22.0f, rc.getWidth() * 0.8f)));
                g.setColour (pl.text.withMultipliedAlpha (a));
                g.drawText (String (st.slice + 1), rc.withTrimmedTop (rc.getHeight() * 0.58f).toNearestInt(), Justification::centred, false);
            }

            // re-triggers : ticks on the top edge
            if (st.roll > 1)
            {
                g.setColour (pl.text.withMultipliedAlpha (a));
                for (int k = 0; k < st.roll; ++k)
                {
                    const float x = tile.getX() + 3.0f + (tile.getWidth() - 6.0f) * ((float) k + 0.5f) / (float) st.roll;
                    g.fillRect (x - 0.75f, tile.getY() + 2.0f, 1.5f, 4.0f);
                }
            }
            // reversed : red corner
            if (st.reverse)
            {
                Path tri;
                tri.addTriangle (tile.getRight() - 9.0f, tile.getBottom() - 2.0f, tile.getRight() - 2.0f, tile.getBottom() - 2.0f, tile.getRight() - 2.0f, tile.getBottom() - 9.0f);
                g.setColour (pl.hot.withMultipliedAlpha (a));
                g.fillPath (tri);
            }
            // per-step effects : dots on the bottom edge
            if (st.fx != 0)
            {
                const Colour fxc[] = { pl.accent, pl.hat, pl.ghost, pl.hot };
                float bx = tile.getX() + 3.0f;
                for (int b = 0; b < bc::kNumStepFx; ++b)
                {
                    if ((st.fx & (1 << b)) == 0) continue;
                    g.setColour (fxc[b].withMultipliedAlpha (a));
                    g.fillEllipse (bx, tile.getBottom() - 6.0f, 4.0f, 4.0f);
                    bx += 5.0f;
                }
            }
            // lock : small bar on the top left
            if (st.locked)
            {
                g.setColour (pl.active);
                g.fillRoundedRectangle (tile.getX() - 1.0f, tile.getY() - 3.0f, jmin (14.0f, tile.getWidth()), 3.0f, 1.0f);
            }
            if (idx == selected)
            {
                g.setColour (pl.text);
                g.drawRoundedRectangle (tile.expanded (1.5f), 5.0f, hasKeyboardFocus (true) ? 2.0f : 1.2f);
            }
        }

        // step number
        const int n = (idx >= bc::kMaxSteps ? idx - bc::kMaxSteps : idx % 16) + 1;
        const bool show = cw >= 15.0f || (n - 1) % 4 == 0;
        if (show)
        {
            g.setFont ((n - 1) % 4 == 0 ? serif (17.0f) : serif (14.0f));
            g.setColour (((n - 1) % 4 == 0 ? pl.text : pl.dim).withMultipliedAlpha (alpha));
            g.drawText (String (n), Rectangle<float> (colX (c) - 4.0f, l.numY, cw + 6.0f, 20.0f).toNearestInt(), Justification::centred);
        }

        // velocity lollipop
        if (st.on)
        {
            const float x = colX (c) + (cw - 2.0f) * 0.5f;
            const float top = l.veloY + 6.0f + (l.veloH - 10.0f) * (1.0f - (float) st.vel / 127.0f);
            const auto vc = breakColour (st.src).withMultipliedAlpha (alpha * (st.src == 1 && ! bActive ? 0.35f : 1.0f));
            g.setColour (vc);
            g.fillRect (x - 1.0f, top, 2.0f, l.veloY + l.veloH - top);
            g.fillEllipse (x - 4.0f, top - 4.0f, 8.0f, 8.0f);
        }
    }

    // ---- empty lanes : what to do
    for (int lane = 0; lane < 2; ++lane)
    {
        if (kits[(size_t) lane] != nullptr) continue;
        Rectangle<float> lr (l.x0, lane == 0 ? 0.0f : l.laneBY, l.w, l.laneH);
        g.setColour (pl.inset.withAlpha (0.85f));
        g.fillRoundedRectangle (lr.reduced (2.0f), 4.0f);
        g.setFont (mono (11.5f));
        g.setColour (pl.dim);
        g.drawText (lane == 0 ? "LOAD A BREAK TO START" : "CLICK HERE TO LOAD A SECOND BREAK  -  ITS HITS WILL FILL THE GAPS OF A",
                    lr.toNearestInt(), Justification::centred);
    }

    // ---- 8-bar map
    const int bars = patternBars();
    for (int b = 0; b <= bc::kMaxBars; ++b)
    {
        auto r = mapBarRect (b);
        const bool isFill = b == bc::kMaxBars;
        const bool used = isFill ? ep.fillEvery > 0 : b < bars;
        const bool shown = isFill ? fillPage : (! fillPage && b >= firstBar && b < firstBar + zoom);
        g.setColour (pl.inset);
        g.fillRoundedRectangle (r, 3.0f);
        const float sw = r.getWidth() / 16.0f;
        for (int s = 0; s < 16; ++s)
        {
            const auto& st = isFill ? pat.fill[(size_t) s] : pat.steps[(size_t) (b * 16 + s)];
            if (! st.on) continue;
            const float y = st.src == 0 ? r.getY() + 3.0f : r.getCentreY() + 1.0f;
            g.setColour (breakColour (st.src).withAlpha (used ? 0.9f : 0.25f));
            g.fillRect (r.getX() + (float) s * sw + 0.5f, y, jmax (1.0f, sw - 1.5f), r.getHeight() * 0.5f - 4.0f);
        }
        const int playBar = playing < 0 ? -1 : (playing >= bc::kMaxSteps ? bc::kMaxBars : playing / 16);
        g.setColour (shown ? pl.text : (playBar == b ? pl.dim : pl.line));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, shown ? 1.8f : 1.0f);
        if (isFill)
        {
            g.setFont (mono (10.0f));
            g.setColour (used ? pl.text : pl.dim);
            g.drawText ("FILL", r.toNearestInt(), Justification::centred);
        }
        if (playBar == b && playing >= 0)
        {
            const int s = playing >= bc::kMaxSteps ? playing - bc::kMaxSteps : playing % 16;
            g.setColour (pl.text);
            g.fillRect (r.getX() + (float) s * sw, r.getBottom() - 2.0f, jmax (2.0f, sw), 2.0f);
        }
    }
}

void StepGrid::paintOverChildren (juce::Graphics& g)
{
    // live read-out while dragging a hit
    if (dragIndex < 0 || (dragMode != DragMode::Gate && dragMode != DragMode::Roll && dragMode != DragMode::Velocity)) return;
    const auto& st = stepops::get (proc, dragIndex);
    juce::String txt = dragMode == DragMode::Gate ? (st.gate >= 0.999f ? juce::String ("LENGTH FULL") : "LENGTH " + juce::String (juce::roundToInt (st.gate * 100)) + "%")
                     : dragMode == DragMode::Roll ? "RETRIG x" + juce::String ((int) st.roll)
                                                  : "VELOCITY " + juce::String ((int) st.vel);
    const int c = dragIndex - firstIndex();
    auto rc = cellRect (st.src, juce::jlimit (0, columns() - 1, c));
    auto box = juce::Rectangle<float> (130.0f, 24.0f).withCentre ({ rc.getCentreX(), rc.getY() + 14.0f });
    box = box.constrainedWithin (getLocalBounds().toFloat());
    g.setColour (pal().text);
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (pal().bg);
    g.setFont (mono (12.0f));
    g.drawText (txt, box.toNearestInt(), juce::Justification::centred);
}

//==============================================================================
void StepGrid::mouseMove (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    int hi = -1, hl = -1;
    if (z == LaneA || z == LaneB) { hi = firstIndex() + colAt (e.position.x); hl = (int) z; }
    if (hi != hoverIndex || hl != hoverLane) { hoverIndex = hi; hoverLane = hl; repaint(); }
    setMouseCursor (z == Map ? juce::MouseCursor::PointingHandCursor : (z == Velo ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor));
}

void StepGrid::mouseExit (const juce::MouseEvent&)
{
    if (hoverIndex >= 0) { hoverIndex = hoverLane = -1; repaint(); }
}

void StepGrid::setVelocityFromY (int index, float y, bool undoable)
{
    const auto l = layout();
    if (! stepops::get (proc, index).on) return;
    const int vel = juce::jlimit (1, 127, (int) std::round (127.0f * (1.0f - (y - l.veloY - 6.0f) / (l.veloH - 10.0f))));
    proc.modifyStep (index, [vel] (bc::Step& s) { s.vel = (uint8_t) vel; }, undoable);
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    dragMode = DragMode::None;
    dragUndoPushed = false;
    veloPainting = false;
    dragIndex = -1;
    downPos = e.position;
    const auto z = zoneAt (e.position);

    if (z == Map)
    {
        for (int b = 0; b <= bc::kMaxBars; ++b)
            if (mapBarRect (b).expanded (2.0f).contains (e.position))
            {
                if (b == bc::kMaxBars) showFill (true);
                else showBar (b);
            }
        return;
    }
    if (z == Velo)
    {
        veloPainting = true;
        const int idx = firstIndex() + colAt (e.position.x);
        setVelocityFromY (idx, e.position.y, true);
        dragIndex = idx;
        return;
    }
    if (z != LaneA && z != LaneB) return;

    const int lane = (int) z;
    const int idx = firstIndex() + colAt (e.position.x);
    const auto st = stepops::get (proc, idx);
    const bool mine = st.on && st.src == lane;

    if (kits[(size_t) lane] == nullptr) { if (onNeedBreak) onNeedBreak (lane); return; }

    if (e.mods.isPopupMenu())
    {
        if (mine) proc.modifyStep (idx, [] (bc::Step& s) { s.reverse = ! s.reverse; });
        else      stepops::addHit (proc, idx, lane, true);
        select (idx);
        return;
    }
    if (e.mods.isShiftDown())
    {
        if (mine) proc.modifyStep (idx, [] (bc::Step& s) { s.locked = ! s.locked; });
        return;
    }
    if (e.mods.isAltDown())
    {
        if (mine) proc.previewSlice (lane, st.slice);
        return;
    }

    if (mine) proc.previewSlice (lane, st.slice);
    else if (! stepops::addHit (proc, idx, lane, false)) return;
    select (idx);
    dragIndex = idx;
    dragStart = stepops::get (proc, idx);
    dragMode = DragMode::Pending;
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (veloPainting)
    {
        if (zoneAt ({ e.position.x, layout().veloY + 4.0f }) != Velo) return;
        const int idx = firstIndex() + colAt (e.position.x);
        setVelocityFromY (idx, e.position.y, false);
        return;
    }
    if (dragIndex < 0 || ! dragStart.on || dragMode == DragMode::None) return;
    const float dx = e.position.x - downPos.x, dy = e.position.y - downPos.y;

    if (dragMode == DragMode::Pending)
    {
        if (std::abs (dx) < 5.0f && std::abs (dy) < 5.0f) return;
        if (e.mods.isCommandDown() || e.mods.isCtrlDown()) dragMode = DragMode::Velocity;
        else dragMode = std::abs (dx) > std::abs (dy) ? DragMode::Gate : DragMode::Roll;
        setMouseCursor (dragMode == DragMode::Gate ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
    }

    std::function<void (bc::Step&)> fn;
    if (dragMode == DragMode::Gate)
    {
        const float gate = juce::jlimit (0.05f, 1.0f, dragStart.gate + dx / (colW() * 2.5f));
        const float snapped = gate > 0.97f ? 1.0f : gate;
        fn = [snapped] (bc::Step& s) { s.gate = snapped; };
    }
    else if (dragMode == DragMode::Roll)
    {
        const int roll = juce::jlimit (1, bc::kMaxRoll, (int) dragStart.roll + (int) std::round (-dy / 14.0f));
        fn = [roll] (bc::Step& s) { s.roll = (uint8_t) roll; };
    }
    else if (dragMode == DragMode::Velocity)
    {
        const int vel = juce::jlimit (1, 127, (int) dragStart.vel - (int) dy);
        fn = [vel] (bc::Step& s) { s.vel = (uint8_t) vel; };
    }
    if (fn)
    {
        proc.modifyStep (dragIndex, fn, ! dragUndoPushed);
        dragUndoPushed = true;
        repaint();
    }
}

void StepGrid::mouseUp (const juce::MouseEvent&)
{
    if (dragIndex >= 0 && dragMode == DragMode::Roll)
    {
        const auto& st = stepops::get (proc, dragIndex);
        proc.previewSlice (st.src, st.slice);
    }
    veloPainting = false;
    dragIndex = -1;
    dragMode = DragMode::None;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void StepGrid::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    if ((z != LaneA && z != LaneB) || e.mods.isPopupMenu()) return;
    const int idx = firstIndex() + colAt (e.position.x);
    const auto& st = stepops::get (proc, idx);
    if (st.on && st.src == (int) z && ! st.locked)
    {
        stepops::clear (proc, idx);
        select (-1);
    }
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto z = zoneAt (e.position);
    if (z != LaneA && z != LaneB) return;
    const int idx = firstIndex() + colAt (e.position.x);
    const auto& st = stepops::get (proc, idx);
    if (! st.on || st.src != (int) z) return;
    select (idx);
    stepops::stepSlice (proc, idx, w.deltaY > 0.0f ? 1 : -1);
}

bool StepGrid::keyPressed (const juce::KeyPress& k)
{
    if (selected < 0) return false;
    const int code = k.getKeyCode();
    if (code == juce::KeyPress::leftKey)  { stepops::stepSlice (proc, selected, -1); return true; }
    if (code == juce::KeyPress::rightKey) { stepops::stepSlice (proc, selected, 1);  return true; }
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey)
    {
        const int d = code == juce::KeyPress::upKey ? 8 : -8;
        proc.modifyStep (selected, [d] (bc::Step& s) { s.vel = (uint8_t) juce::jlimit (1, 127, (int) s.vel + d); });
        return true;
    }
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        stepops::clear (proc, selected);
        select (-1);
        return true;
    }
    if (code == juce::KeyPress::escapeKey) { select (-1); return true; }
    return false;
}
