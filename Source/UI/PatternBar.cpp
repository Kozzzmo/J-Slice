#include "PatternBar.h"
#include "LookAndFeel.h"

using namespace juce;
using namespace ui;

static constexpr int kMaxChips = 12;

Rectangle<int> PatternBar::slotRect (int i) const
{
    const int w = 34;
    return { 64 + i * (w + 3), 0, w, kRowH };
}
Rectangle<int> PatternBar::btnRect (int i) const   { return { getWidth() - 3 * 62 + i * 62, 0, 58, kRowH }; }
Rectangle<int> PatternBar::chainBtn() const        { return { 0, kRowH + kGap, 58, kRowH }; }
Rectangle<int> PatternBar::chipRect (int i) const  { return { 64 + i * 56, kRowH + kGap, 52, kRowH }; }
Rectangle<int> PatternBar::addRect() const
{
    const int n = jmin (kMaxChips, (int) proc.getChain().size());
    return { 64 + n * 56, kRowH + kGap, 52, kRowH };
}
Rectangle<int> PatternBar::clrRect() const         { return { getWidth() - 58, kRowH + kGap, 58, kRowH }; }

static void cell (Graphics& g, Rectangle<int> r, const String& text, bool on, Colour onColour, bool dimText = false)
{
    const auto& pl = pal();
    g.setColour (on ? onColour : pl.raised);
    g.fillRoundedRectangle (r.toFloat(), 3.0f);
    g.setColour (on ? onColour : pl.line);
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3.0f, 1.0f);
    g.setColour (on ? pl.onAccent : (dimText ? pl.dim : pl.text));
    g.setFont (mono (10.5f, true, 0.06f));
    g.drawText (text, r, Justification::centred);
}

void PatternBar::paint (Graphics& g)
{
    const auto& pl = pal();
    g.setFont (mono (10.0f));
    g.setColour (pl.dim);
    g.drawText ("PATTERN", 0, 0, 60, kRowH, Justification::centredLeft);

    const int edit = proc.getEditSlot(), play = proc.getPlaySlot(), queued = proc.getQueuedSlot();
    const bool running = proc.isRunning();
    for (int i = 0; i < JSliceProcessor::kSlots; ++i)
    {
        const auto r = slotRect (i);
        cell (g, r, slotName (i), i == edit, pl.accent, ! proc.slotUsed (i));
        if (proc.slotUsed (i) && i != edit) { g.setColour (pl.dim); g.fillEllipse (r.getX() + 3.0f, r.getY() + 3.0f, 3.5f, 3.5f); }
        if (running && i == play)    { g.setColour (pl.hot); g.fillRect (r.getX() + 4, r.getBottom() - 4, r.getWidth() - 8, 2); }
        if (queued == i)             { g.setColour (pl.hot); g.drawRoundedRectangle (r.toFloat().reduced (1.0f), 3.0f, 1.5f); }
    }
    cell (g, btnRect (0), proc.hasSlotClip() ? "COPIED" : "COPY", false, pl.accent);
    cell (g, btnRect (1), "PASTE", false, pl.accent, ! proc.hasSlotClip());
    cell (g, btnRect (2), "CLEAR", false, pl.accent);

    // chain
    const auto& ch = proc.getChain();
    cell (g, chainBtn(), "CHAIN", proc.isChainOn(), pl.hot, ch.empty());
    for (int i = 0; i < jmin (kMaxChips, (int) ch.size()); ++i)
    {
        const auto r = chipRect (i);
        const bool now = proc.isChainOn() && running && ch[(size_t) i].slot == play && i == (proc.getChainPos() + (int) ch.size() - (queued >= 0 ? 1 : 0)) % (int) ch.size();
        String t = slotName (ch[(size_t) i].slot);
        if (ch[(size_t) i].repeats > 1) t << " x" << ch[(size_t) i].repeats;
        cell (g, r, t, now, pl.active);
        if (i > 0)
        {
            g.setColour (pl.dim);
            g.drawText (">", r.getX() - 5, r.getY(), 5, r.getHeight(), Justification::centred);
        }
    }
    if ((int) ch.size() < kMaxChips)
        cell (g, addRect(), "+" + slotName (edit), false, pl.accent, true);
    cell (g, clrRect(), "CLR", false, pl.accent, ch.empty());
}

void PatternBar::mouseDown (const MouseEvent& e)
{
    const auto p = e.getPosition();
    for (int i = 0; i < JSliceProcessor::kSlots; ++i)
        if (slotRect (i).contains (p)) { proc.selectSlot (i); repaint(); return; }
    if (btnRect (0).contains (p)) { proc.copySlot(); repaint(); return; }
    if (btnRect (1).contains (p)) { proc.pasteSlot(); repaint(); return; }
    if (btnRect (2).contains (p)) { proc.clearSlot(); repaint(); return; }

    auto ch = proc.getChain();
    if (chainBtn().contains (p)) { proc.setChainOn (! proc.isChainOn()); repaint(); return; }
    for (int i = 0; i < jmin (kMaxChips, (int) ch.size()); ++i)
        if (chipRect (i).contains (p))
        {
            // click = one more repeat (1..8), right-click = remove
            if (e.mods.isPopupMenu()) ch.erase (ch.begin() + i);
            else ch[(size_t) i].repeats = ch[(size_t) i].repeats % 8 + 1;
            proc.setChain (ch); repaint(); return;
        }
    if ((int) ch.size() < kMaxChips && addRect().contains (p)) { ch.push_back ({ proc.getEditSlot(), 1 }); proc.setChain (ch); repaint(); return; }
    if (clrRect().contains (p)) { proc.setChainOn (false); proc.setChain ({}); repaint(); return; }
}
