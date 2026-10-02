#include "WaveformView.h"
#include "../PluginProcessor.h"

using namespace ui;

WaveformView::WaveformView (JSliceProcessor& p) : proc (p)
{
    setRepaintsOnMouseActivity (false);
    refresh();
}

void WaveformView::refresh()
{
    kit = proc.getKit();
    if (kit == nullptr || selected >= (int) kit->slices.size()) selected = -1;
    rebuildPeaks();
    repaint();
}

void WaveformView::tick()
{
    const int s = proc.engine.uiSlice.load();
    const double pos = proc.engine.uiSamplePos.load();
    if (s != playingSlice || std::abs (pos - playPos) > 1.0)
    {
        playingSlice = s;
        playPos = pos;
        repaint (waveArea().expanded (2));
    }
}

juce::Rectangle<int> WaveformView::waveArea() const
{
    return getLocalBounds().reduced (10, 6).withTrimmedTop (18).withTrimmedBottom (16);
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

void WaveformView::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    g.setColour (col::panelEdge);
    g.fillRoundedRectangle (r, 6.0f);
    auto lcd = r.reduced (3.0f);
    ColourGradient grad (col::lcdBg, lcd.getX(), lcd.getY(), col::lcdBg2, lcd.getX(), lcd.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (lcd, 4.0f);
    // LCD pixel texture
    g.setColour (col::lcdInk.withAlpha (0.035f));
    for (float y = lcd.getY(); y < lcd.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, lcd.getX(), lcd.getRight());

    const auto a = waveArea();
    g.setColour (col::lcdInk);

    if (kit == nullptr || kit->audio == nullptr)
    {
        g.setFont (monoFont (20.0f));
        g.drawText ("DROP A BREAK HERE", a.withTrimmedBottom (a.getHeight() / 2), Justification::centredBottom);
        g.setFont (monoFont (13.0f, false));
        g.drawText ("or click BROWSE  -  WAV / AIFF / FLAC / MP3 / OGG", a.withTrimmedTop (a.getHeight() / 2 + 6), Justification::centredTop);
        return;
    }

    const double sps = kit->samplesPerStep();
    const int64_t len = kit->length();

    // beat grid
    for (int st = 0; ; ++st)
    {
        const double s = (double) kit->gridOrigin + st * sps;
        if (s >= (double) len) break;
        const float x = sampleToX (s);
        const bool bar = st % 16 == 0, beat = st % 4 == 0;
        g.setColour (col::lcdInk.withAlpha (bar ? 0.35f : beat ? 0.18f : 0.07f));
        g.drawVerticalLine ((int) x, (float) a.getY(), (float) a.getBottom());
        if (bar)
        {
            g.setFont (monoFont (10.0f));
            g.drawText (String (st / 16 + 1), (int) x + 2, a.getBottom() + 1, 20, 12, Justification::left);
        }
    }

    // slice highlights
    for (int i = 0; i < (int) kit->slices.size(); ++i)
    {
        const auto& sl = kit->slices[(size_t) i];
        const float x0 = sampleToX ((double) sl.start), x1 = sampleToX ((double) sl.end);
        if (i == playingSlice)
        {
            g.setColour (col::lcdInk.withAlpha (0.22f));
            g.fillRect (Rectangle<float> (x0, (float) a.getY(), x1 - x0, (float) a.getHeight()));
        }
        else if (i == selected)
        {
            g.setColour (col::lcdInk.withAlpha (0.10f));
            g.fillRect (Rectangle<float> (x0, (float) a.getY(), x1 - x0, (float) a.getHeight()));
        }
    }

    // waveform
    g.setColour (col::lcdInk.withAlpha (0.9f));
    const float mid = (float) a.getCentreY(), half = a.getHeight() * 0.48f;
    for (int x = 0; x < (int) peaks.size(); ++x)
    {
        const auto [lo, hi] = peaks[(size_t) x];
        g.drawVerticalLine (a.getX() + x, mid - hi * half, mid - lo * half + 1.0f);
    }

    // markers + tags (full tag when there is room, otherwise a small coloured flag)
    for (int i = 0; i < (int) kit->slices.size(); ++i)
    {
        const auto& sl = kit->slices[(size_t) i];
        const float x = sampleToX ((double) sl.start);
        const float nextX = i + 1 < (int) kit->slices.size() ? sampleToX ((double) kit->slices[(size_t) i + 1].start) : (float) a.getRight();
        const bool hot = i == hoverMarker || i == dragMarker || i == selected;
        g.setColour (col::lcdInk.withAlpha (hot ? 1.0f : 0.7f));
        g.fillRect (Rectangle<float> (x - (hot ? 1.0f : 0.5f), (float) a.getY() - 2.0f, hot ? 2.0f : 1.0f, (float) a.getHeight() + 2.0f));

        const String txt = String (hitClassLetter (sl.cls)) + String (i + 1);
        const float fullW = 6.0f + 6.0f * (float) txt.length();
        const bool room = nextX - x >= fullW + 1.0f || hot;
        auto tag = Rectangle<float> (x - 0.5f, (float) a.getY() - 17.0f, room ? fullW : juce::jmax (3.0f, juce::jmin (6.0f, nextX - x - 1.0f)), 14.0f);
        g.setColour (col::lcdInk);
        g.fillRect (tag);
        g.setColour (classColour (sl.cls));
        g.fillRect (tag.withWidth (juce::jmin (4.0f, tag.getWidth())));
        if (room)
        {
            g.setColour (col::lcdBg);
            g.setFont (monoFont (9.5f));
            g.drawText (txt, tag.withTrimmedLeft (4.0f).toNearestInt(), Justification::centred);
        }
    }

    // play head
    if (playPos >= 0.0)
    {
        g.setColour (col::accent);
        g.fillRect (Rectangle<float> (sampleToX (playPos) - 1.0f, (float) a.getY(), 2.0f, (float) a.getHeight()));
    }

    // info line
    g.setColour (col::lcdInk);
    g.setFont (monoFont (11.0f));
    String info;
    info << (int) kit->slices.size() << " SLICES   " << String (kit->bpm, 2) << " BPM   " << kit->bars << (kit->bars > 1 ? " BARS   " : " BAR   ")
         << String (kit->audio->sampleRate / 1000.0, 1) << " kHz   " << String ((double) len / kit->audio->sampleRate, 2) << " s";
    g.drawText (info, a.getX(), a.getBottom() + 2, a.getWidth(), 12, Justification::right);
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    const int m = markerAt ((float) e.x);
    if (m != hoverMarker)
    {
        hoverMarker = m;
        setMouseCursor (m >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    if (kit == nullptr) return;
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
        dragMarker = m;
        dragSlices = kit->slices;
        return;
    }
    selected = sliceAt ((float) e.x);
    if (selected >= 0) proc.previewSlice (selected);
    repaint();
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMarker < 0 || kit == nullptr) return;
    const auto i = (size_t) dragMarker;
    int64_t lo = i > 0 ? dragSlices[i - 1].start + 64 : 0;
    int64_t hi = i + 1 < dragSlices.size() ? dragSlices[i + 1].start - 64 : kit->length() - 64;
    dragSlices[i].start = juce::jlimit (lo, juce::jmax (lo, hi), (int64_t) xToSample ((float) e.x));
    proc.updateSlices (dragSlices, false);
}

void WaveformView::mouseUp (const juce::MouseEvent&)
{
    if (dragMarker >= 0)
    {
        proc.updateSlices (dragSlices, true);
        proc.previewSlice (dragMarker);
        dragMarker = -1;
    }
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (kit == nullptr || (int) kit->slices.size() >= bc::kMaxSlices) return;
    if (markerAt ((float) e.x) >= 0) return;
    auto slices = kit->slices;
    bc::Slice s;
    s.start = (int64_t) xToSample ((float) e.x);
    s.strength = 0.5f;
    slices.push_back (s);
    proc.updateSlices (slices, true);
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
        if (r == 1) { self.proc.previewSlice (slice); return; }
        if (r >= 10 && r < 10 + bc::kNumClasses)
        {
            slices[(size_t) slice].cls = (bc::HitClass) (r - 10);
            slices[(size_t) slice].userClass = true;
            self.proc.updateSlices (slices, false);
        }
        else if (r == 20)
        {
            slices[(size_t) slice].userClass = false;
            self.proc.updateSlices (slices, true);
        }
        else if (r == 30)
        {
            slices.erase (slices.begin() + slice);
            self.proc.updateSlices (slices, true);
        }
    });
}
