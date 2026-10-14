#include "WaveformView.h"
#include "../PluginProcessor.h"

using namespace ui;

WaveformView::WaveformView (JSliceProcessor& p, int s) : proc (p), slot (s)
{
    setRepaintsOnMouseActivity (false);
    setTooltip ("Click: listen to a slice  |  drag a marker: move it\n"
                "Double-click a slice tag (top strip): delete that slice  |  double-click the waveform: add a marker\n"
                "Right-click: change the drum class, delete");
    refresh();
}

void WaveformView::refresh()
{
    kit = proc.getKit (slot);
    if (kit == nullptr || selected >= (int) kit->slices.size()) selected = -1;
    rebuildPeaks();
    repaint();
}

void WaveformView::tick()
{
    const int s = proc.engine.uiSlice[slot].load();
    const double pos = proc.engine.uiSamplePos[slot].load();
    if (s != playingSlice || std::abs (pos - playPos) > 1.0)
    {
        playingSlice = s;
        playPos = pos;
        repaint (waveArea().expanded (2));
    }
}

juce::Rectangle<int> WaveformView::waveArea() const
{
    return getLocalBounds().reduced (6, 4).withTrimmedTop (18);
}

float WaveformView::sampleToX (double s) const
{
    const auto a = waveArea();
    if (kit == nullptr || kit->length() <= 0) return (float) a.getX();
    return (float) a.getX() + (float) (s / (double) kit->length() * a.getWidth());
}

double WaveformView::xToSample (float x) const
{
    const auto a = waveArea();
    if (kit == nullptr) return 0.0;
    return juce::jlimit (0.0, (double) kit->length() - 1.0, (double) (x - (float) a.getX()) / (double) a.getWidth() * (double) kit->length());
}

int WaveformView::markerAt (float x) const
{
    if (kit == nullptr) return -1;
    int best = -1; float bestD = 6.0f;
    for (int i = 0; i < (int) kit->slices.size(); ++i)
    {
        const float d = std::abs (sampleToX ((double) kit->slices[(size_t) i].start) - x);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

int WaveformView::sliceAt (float x) const
{
    if (kit == nullptr) return -1;
    const double s = xToSample (x);
    for (int i = (int) kit->slices.size() - 1; i >= 0; --i)
        if ((double) kit->slices[(size_t) i].start <= s) return i;
    return kit->slices.empty() ? -1 : 0;
}

void WaveformView::rebuildPeaks()
{
    peaks.clear();
    if (kit == nullptr || kit->audio == nullptr) return;
    const auto a = waveArea();
    const auto& b = kit->audio->buffer;
    const int w = juce::jmax (1, a.getWidth());
    const int64_t n = b.getNumSamples();
    peaks.resize ((size_t) w);
    for (int x = 0; x < w; ++x)
    {
        const int64_t s0 = n * x / w, s1 = juce::jmax (s0 + 1, n * (x + 1) / w);
        float lo = 0.0f, hi = 0.0f;
        for (int64_t i = s0; i < s1; ++i)
        {
            const float v = 0.5f * (b.getSample (0, (int) i) + b.getSample (1, (int) i));
            lo = juce::jmin (lo, v); hi = juce::jmax (hi, v);
        }
        peaks[(size_t) x] = { lo, hi };
    }
}

juce::Rectangle<float> WaveformView::tagRect (int i, bool& full) const
{
    const auto a = waveArea();
    const auto& sl = kit->slices[(size_t) i];
    const float x = sampleToX ((double) sl.start);
    const float nextX = i + 1 < (int) kit->slices.size() ? sampleToX ((double) kit->slices[(size_t) i + 1].start) : (float) a.getRight();
    const float fullW = 10.0f + 7.8f * (float) (juce::String (i + 1).length() + 1);
    full = nextX - x >= fullW + 1.0f || i == hoverMarker || i == selected;
    return { x, (float) a.getY() - 17.0f, full ? fullW : juce::jmax (4.0f, juce::jmin (8.0f, nextX - x - 1.0f)), 14.0f };
}

int WaveformView::tagAt (juce::Point<float> p) const
{
    if (kit == nullptr) return -1;
    const auto a = waveArea();
    if (p.y >= (float) a.getY()) return -1;
    // the visible tag first, then the slice region under the mouse
    for (int i = (int) kit->slices.size() - 1; i >= 0; --i)
    {
        bool full = false;
        if (tagRect (i, full).expanded (1.0f, 3.0f).contains (p)) return i;
    }
    return sliceAt (p.x);
}

void WaveformView::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto& pl = pal();
    auto r = getLocalBounds().toFloat();
    g.setColour (pl.inset);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (pl.line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    const auto a = waveArea();

    if (kit == nullptr || kit->audio == nullptr)
    {
        g.setColour (breakColour (slot).withAlpha (0.9f));
        g.setFont (serifItalic (28.0f));
        auto t = getLocalBounds().reduced (12, 6);
        g.drawText (slot == 0 ? "Drop a break here" : "Drop a second break here", t.removeFromTop (t.getHeight() / 2 + 6), Justification::centredBottom);
        g.setFont (mono (11.0f));
        g.setColour (pl.dim);
        g.drawText (slot == 0 ? "OR CLICK TO OPEN  -  WAV / AIFF / FLAC / MP3 / OGG"
                              : "BREAK B FILLS THE GAPS OF BREAK A  -  CLICK TO OPEN", t.withTrimmedTop (4), Justification::centredTop);
        return;
    }

    const double sps = kit->samplesPerStep();
    const int64_t len = kit->length();

    // alternate slice shading + playing / selected slice
    for (int i = 0; i < (int) kit->slices.size(); ++i)
    {
        const auto& sl = kit->slices[(size_t) i];
        const float x0 = sampleToX ((double) sl.start), x1 = sampleToX ((double) sl.end);
        Rectangle<float> band (x0, (float) a.getY(), x1 - x0, (float) a.getHeight());
        if (i == playingSlice)      g.setColour (breakColour (slot).withAlpha (0.22f));
        else if (i == selected)     g.setColour (breakColour (slot).withAlpha (0.10f));
        else if (i % 2 == 1)        g.setColour (pl.raised.withAlpha (0.55f));
        else continue;
        g.fillRect (band);
    }

    // beat grid
    for (int st = 0; ; ++st)
    {
        const double s = (double) kit->gridOrigin + st * sps;
        if (s >= (double) len) break;
        const float x = sampleToX (s);
        const bool bar = st % 16 == 0, beat = st % 4 == 0;
        g.setColour (bar ? pl.line : pl.faint.withAlpha (beat ? 0.9f : 0.35f));
        if (bar || beat) g.drawVerticalLine ((int) x, (float) a.getY(), (float) a.getBottom());
        else for (float y = (float) a.getY(); y < (float) a.getBottom(); y += 4.0f) g.fillRect (x, y, 1.0f, 1.0f);
    }

    // waveform (filled, symmetric)
    const float mid = (float) a.getCentreY(), half = a.getHeight() * 0.47f;
    Path wave;
    if (! peaks.empty())
    {
        wave.startNewSubPath ((float) a.getX(), mid);
        for (int x = 0; x < (int) peaks.size(); ++x) wave.lineTo ((float) (a.getX() + x), mid - peaks[(size_t) x].second * half);
        for (int x = (int) peaks.size() - 1; x >= 0; --x) wave.lineTo ((float) (a.getX() + x), mid - peaks[(size_t) x].first * half);
        wave.closeSubPath();
        g.setColour (breakColour (slot).withAlpha (dimmed ? 0.3f : 0.85f));
        g.fillPath (wave);
    }
    g.setColour (pl.faint);
    g.drawHorizontalLine ((int) mid, (float) a.getX(), (float) a.getRight());

    // markers + tags
    for (int i = 0; i < (int) kit->slices.size(); ++i)
    {
        const auto& sl = kit->slices[(size_t) i];
        const float x = sampleToX ((double) sl.start);
        const bool hot = i == hoverMarker || i == dragMarker || i == selected;
        const auto cc = classColour (sl.cls);
        g.setColour (hot ? pl.text : cc.withAlpha (0.75f));
        g.fillRect (Rectangle<float> (x - (hot ? 1.0f : 0.5f), (float) a.getY() - 3.0f, hot ? 2.0f : 1.0f, (float) a.getHeight() + 3.0f));

        bool full = false;
        auto tag = tagRect (i, full);
        g.setColour (cc);
        g.fillRoundedRectangle (tag, 2.5f);
        if (full)
        {
            g.setColour (pl.onAccent);
            g.setFont (mono (10.5f, true, 0.0f));
            g.drawText (String (hitClassLetter (sl.cls)) + String (i + 1), tag.toNearestInt(), Justification::centred);
        }
    }

    // play head
    if (playPos >= 0.0)
    {
        g.setColour (pl.text);
        g.fillRect (Rectangle<float> (sampleToX (playPos) - 1.0f, (float) a.getY(), 2.0f, (float) a.getHeight()));
    }

}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    const int m = e.position.y < (float) waveArea().getY() ? tagAt (e.position) : markerAt ((float) e.x);
    if (m != hoverMarker)
    {
        hoverMarker = m;
        setMouseCursor (m >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    if (onActivate) onActivate();
    if (kit == nullptr) { if (onEmptyClick) onEmptyClick(); return; }
    if (e.mods.isPopupMenu())
    {
        const int m = markerAt ((float) e.x);
        const int s = m >= 0 ? m : sliceAt ((float) e.x);
        if (s >= 0) showSliceMenu (s);
        return;
    }
    const int m = markerAt ((float) e.x);
    if (m >= 0)
    {
        markerMoved = false;
        dragMarker = m;
        dragSlices = kit->slices;
        return;
    }
    selected = sliceAt ((float) e.x);
    if (selected >= 0) proc.previewSlice (slot, selected);
    repaint();
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMarker < 0 || kit == nullptr || e.getDistanceFromDragStart() < 3) return;
    markerMoved = true;
    const auto i = (size_t) dragMarker;
    int64_t lo = i > 0 ? dragSlices[i - 1].start + 64 : 0;
    int64_t hi = i + 1 < dragSlices.size() ? dragSlices[i + 1].start - 64 : kit->length() - 64;
    dragSlices[i].start = juce::jlimit (lo, juce::jmax (lo, hi), (int64_t) xToSample ((float) e.x));
    proc.updateSlices (slot, dragSlices, false);
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    if (dragMarker >= 0)
    {
        if (markerMoved) proc.updateSlices (slot, dragSlices, true);
        proc.previewSlice (slot, dragMarker);
        selected = dragMarker;
        dragMarker = -1;
        markerMoved = false;
        repaint();
    }
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (kit == nullptr) return;

    // top strip (slice tags) : delete the slice
    const int tag = tagAt (e.position);
    if (tag >= 0)
    {
        if (kit->slices.size() <= 1) return;
        auto slices = kit->slices;
        slices.erase (slices.begin() + tag);
        selected = -1;
        hoverMarker = -1;
        proc.updateSlices (slot, slices, true);
        return;
    }

    // waveform : add a marker
    if ((int) kit->slices.size() >= bc::kMaxSlices || markerAt ((float) e.x) >= 0) return;
    auto slices = kit->slices;
    bc::Slice s;
    s.start = (int64_t) xToSample ((float) e.x);
    s.strength = 0.5f;
    slices.push_back (s);
    proc.updateSlices (slot, slices, true);
}

void WaveformView::showSliceMenu (int slice)
{
    using namespace juce;
    if (kit == nullptr || slice >= (int) kit->slices.size()) return;
    const auto cls = kit->slices[(size_t) slice].cls;
    PopupMenu m;
    m.addSectionHeader ("Slice " + String (slice + 1));
    m.addItem (1, "Preview");
    m.addSeparator();
    for (int c = 0; c < bc::kNumClasses; ++c)
        m.addItem (10 + c, String ("Set as ") + bc::hitClassName ((bc::HitClass) c), true, (int) cls == c);
    m.addItem (20, "Automatic class", kit->slices[(size_t) slice].userClass);
    m.addSeparator();
    m.addItem (30, "Delete marker", kit->slices.size() > 1);

    SafePointer<WaveformView> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, slice] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& self = *safe;
        if (self.kit == nullptr || slice >= (int) self.kit->slices.size()) return;
        auto slices = self.kit->slices;
        if (r == 1) { self.proc.previewSlice (self.slot, slice); return; }
        if (r >= 10 && r < 10 + bc::kNumClasses)
        {
            slices[(size_t) slice].cls = (bc::HitClass) (r - 10);
            slices[(size_t) slice].userClass = true;
            self.proc.updateSlices (self.slot, slices, false);
        }
        else if (r == 20)
        {
            slices[(size_t) slice].userClass = false;
            self.proc.updateSlices (self.slot, slices, true);
        }
        else if (r == 30)
        {
            slices.erase (slices.begin() + slice);
            self.proc.updateSlices (self.slot, slices, true);
        }
    });
}
