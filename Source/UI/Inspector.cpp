#include "Inspector.h"
#include "../PluginProcessor.h"

using namespace ui;

namespace
{
    constexpr int kRolls[] = { 1, 2, 3, 4, 6, 8 };
}

Inspector::Inspector (JSliceProcessor& p) : proc (p)
{
    using namespace juce;
    srcBtn.setTooltip ("Which break plays this step. Click to move the hit to the other break.");
    srcBtn.onClick = [this] { if (hasStep()) stepops::switchBreak (proc, index); };
    lockBtn.setClickingTogglesState (false);
    lockBtn.setTooltip ("Locked steps are kept by GENERATE, MUTATE and the macros");
    lockBtn.onClick = [this] { modify ([] (bc::Step& s) { s.locked = ! s.locked; }); };
    prevBtn.setTooltip ("Previous slice (left arrow key)");
    nextBtn.setTooltip ("Next slice (right arrow key)");
    prevBtn.onClick = [this] { if (hasStep()) stepops::stepSlice (proc, index, -1); };
    nextBtn.onClick = [this] { if (hasStep()) stepops::stepSlice (proc, index, 1); };

    lengthBar.get = [this] { return hasStep() ? (double) step().gate : 1.0; };
    lengthBar.set = [this] (double v, bool first)
    {
        const float gate = v > 0.97 ? 1.0f : juce::jmax (0.05f, (float) v);
        modify ([gate] (bc::Step& s) { s.gate = gate; }, first);
    };
    lengthBar.format = [] (double v) { return v > 0.97 ? String ("Full") : String (roundToInt (v * 100.0)) + "%"; };
    lengthBar.setTooltip ("How long the hit plays (Full = until the next hit)");
    velBar.get = [this] { return hasStep() ? (double) step().vel / 127.0 : 0.0; };
    velBar.set = [this] (double v, bool first)
    {
        const int vel = jlimit (1, 127, roundToInt (v * 127.0));
        modify ([vel] (bc::Step& s) { s.vel = (uint8_t) vel; }, first);
    };
    velBar.format = [] (double v) { return String (jlimit (1, 127, roundToInt (v * 127.0))); };
    velBar.onRelease = [this] { if (hasStep()) proc.previewSlice (step().src, step().slice); };

    for (int r : kRolls)
    {
        auto* b = rollBtns.add (new TextButton (String (r)));
        b->setTooltip (r == 1 ? String ("Single hit") : "Re-trigger the slice " + String (r) + " times inside the step");
        b->onClick = [this, r] { modify ([r] (bc::Step& s) { s.roll = (uint8_t) r; }); if (hasStep()) proc.previewSlice (step().src, step().slice); };
    }
    const char* fxNames[] = { "Dly", "Rev", "Lo-fi", "Filter" };
    const char* fxTips[] = { "Send this hit into the delay (SPACE tab)", "Send this hit into the reverb (SPACE tab)",
                             "8 kHz / 5-bit crunch on this hit only", "Telephone band-pass on this hit only" };
    for (int b = 0; b < bc::kNumStepFx; ++b)
    {
        auto* btn = fxBtns.add (new TextButton (fxNames[b]));
        btn->setTooltip (fxTips[b]);
        btn->onClick = [this, b] { if (hasStep()) stepops::toggleFx (proc, index, (uint8_t) (1 << b)); };
    }
    pitchDown.onClick = [this] { modify ([] (bc::Step& s) { s.pitch = (int8_t) jlimit (-24, 24, s.pitch - 1); }); if (hasStep()) proc.previewSlice (step().src, step().slice); };
    pitchUp.onClick   = [this] { modify ([] (bc::Step& s) { s.pitch = (int8_t) jlimit (-24, 24, s.pitch + 1); }); if (hasStep()) proc.previewSlice (step().src, step().slice); };
    reverseBtn.onClick = [this] { modify ([] (bc::Step& s) { s.reverse = ! s.reverse; }); };
    clearBtn.setTooltip ("Delete this hit (Delete key, or double-click it in the lane)");
    clearBtn.onClick = [this] { if (hasStep()) stepops::clear (proc, index); };

    for (auto* c : std::initializer_list<Component*> { &srcBtn, &lockBtn, &prevBtn, &nextBtn, &pitchDown, &pitchUp, &reverseBtn, &clearBtn, &lengthBar, &velBar })
        addChildComponent (c);
    for (auto* b : rollBtns) addChildComponent (b);
    for (auto* b : fxBtns) addChildComponent (b);
    refresh();
}

const bc::Step& Inspector::step() const { return stepops::get (proc, index); }
bool Inspector::hasStep() const         { return index >= 0 && step().on; }

void Inspector::modify (std::function<void (bc::Step&)> fn, bool undoable)
{
    if (! hasStep()) return;
    proc.modifyStep (index, fn, undoable);
}

void Inspector::setIndex (int i)
{
    index = i;
    refresh();
}

void Inspector::refresh()
{
    using namespace juce;
    const bool has = hasStep();
    for (auto* c : getChildren()) c->setVisible (has);
    if (has)
    {
        const auto& st = step();
        const auto col = breakColour (st.src);
        srcBtn.setButtonText (st.src == 0 ? "A" : "B");
        srcBtn.setColour (TextButton::buttonColourId, col);
        srcBtn.setColour (TextButton::textColourOffId, pal().onAccent);
        srcBtn.setEnabled (proc.getKit (1 - st.src) != nullptr);
        lockBtn.setToggleState (st.locked, dontSendNotification);
        reverseBtn.setToggleState (st.reverse, dontSendNotification);
        for (int i = 0; i < rollBtns.size(); ++i) rollBtns[i]->setToggleState (st.roll == kRolls[i], dontSendNotification);
        for (int b = 0; b < fxBtns.size(); ++b) fxBtns[b]->setToggleState ((st.fx & (1 << b)) != 0, dontSendNotification);
        lengthBar.colour = col;
        velBar.colour = col;

        auto k = proc.getKit (st.src);
        if (k != envKit || st.slice != envSlice)
        {
            envKit = k;
            envSlice = st.slice;
            env = k != nullptr ? stepops::envelope (*k, st.slice, 120) : std::vector<float>();
        }
    }
    repaint();
    lengthBar.repaint();
    velBar.repaint();
}

juce::Rectangle<int> Inspector::waveArea() const
{
    return { 34, 34, getWidth() - 68, 64 };
}

void Inspector::resized()
{
    const int w = getWidth();
    srcBtn.setBounds (128, 2, 34, 26);
    lockBtn.setBounds (w - 72, 2, 72, 26);
    prevBtn.setBounds (0, 40, 28, 52);
    nextBtn.setBounds (w - 28, 40, 28, 52);
    clearBtn.setBounds (w - 72, 106, 72, 26);
    lengthBar.setBounds (0, 140, w, 26);
    {
        const int x0 = 96, bw = (w - x0 - 5 * 4) / 6;
        for (int i = 0; i < rollBtns.size(); ++i) rollBtns[i]->setBounds (x0 + i * (bw + 4), 172, bw, 26);
    }
    velBar.setBounds (0, 204, w, 26);
    pitchDown.setBounds (96, 236, 30, 26);
    pitchUp.setBounds (184, 236, 30, 26);
    reverseBtn.setBounds (w - 96, 236, 96, 26);
    {
        const int bw = (w - 3 * 6) / 4;
        for (int b = 0; b < fxBtns.size(); ++b) fxBtns[b]->setBounds (b * (bw + 6), 272, bw, 28);
    }
}

void Inspector::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto& pl = pal();
    const int w = getWidth();

    if (! hasStep())
    {
        g.setFont (mono (11.0f));
        g.setColour (pl.dim);
        g.drawText ("STEP", 0, 2, 120, 26, Justification::centredLeft);
        g.setFont (serifItalic (30.0f));
        g.setColour (pl.text);
        g.drawText ("Select a hit", getLocalBounds().withTrimmedTop (50).removeFromTop (40), Justification::centred);
        g.setFont (mono (11.0f));
        g.setColour (pl.dim);
        g.drawFittedText ("EMPTY CELL : ADD A HIT\nHIT : SELECT AND EDIT IT HERE\n\n"
                          "DOUBLE-CLICK / DELETE : REMOVE\nRIGHT-CLICK : REVERSED HIT\nWHEEL / ARROWS : CHANGE THE SLICE",
                          getLocalBounds().withTrimmedTop (100).reduced (6, 0).removeFromTop (150), Justification::centredTop, 8);
        return;
    }

    const auto& st = step();
    const auto col = breakColour (st.src);
    const bool fill = index >= bc::kMaxSteps;
    const int bar = fill ? 0 : index / 16, s = (fill ? index - bc::kMaxSteps : index % 16) + 1;

    g.setFont (mono (12.0f, true, 0.18f));
    g.setColour (pl.dim);
    g.drawText (fill ? "FILL  " + String (s).paddedLeft ('0', 2) : "STEP  " + String (bar + 1) + String (CharPointer_UTF8 (" \xc2\xb7 ")) + String (s).paddedLeft ('0', 2),
                0, 2, 124, 26, Justification::centredLeft);

    // slice waveform
    auto wa = waveArea().toFloat();
    g.setColour (pl.inset);
    g.fillRoundedRectangle (wa.expanded (30.0f, 0.0f), 5.0f);
    g.setColour (pl.line);
    g.drawRoundedRectangle (wa.expanded (30.0f, 0.0f).reduced (0.5f), 5.0f, 1.0f);
    if (! env.empty())
    {
        auto ga = wa.reduced (6.0f, 6.0f);
        const float mid = ga.getCentreY();
        const int n = (int) env.size();
        Path p;
        p.startNewSubPath (ga.getX(), mid);
        for (int i = 0; i < n; ++i)
            p.lineTo (ga.getX() + ga.getWidth() * (float) i / (float) (n - 1), mid - env[(size_t) (st.reverse ? n - 1 - i : i)] * ga.getHeight() * 0.5f);
        for (int i = n - 1; i >= 0; --i)
            p.lineTo (ga.getX() + ga.getWidth() * (float) i / (float) (n - 1), mid + env[(size_t) (st.reverse ? n - 1 - i : i)] * ga.getHeight() * 0.5f);
        p.closeSubPath();
        g.setColour (col);
        g.fillPath (p);
        // the played part (length)
        if (st.gate < 0.999f)
        {
            g.setColour (pl.inset.withAlpha (0.7f));
            g.fillRect (ga.withTrimmedLeft (ga.getWidth() * st.gate));
        }
    }

    // slice name
    auto k = proc.getKit (st.src);
    const int nSlices = k != nullptr ? (int) k->slices.size() : 0;
    const auto cls = (k != nullptr && st.slice < nSlices) ? k->slices[(size_t) st.slice].cls : st.role;
    String name = bc::hitClassName (cls);
    if (name.contains ("/")) name = name.upToFirstOccurrenceOf (" /", false, false);
    g.setFont (serif (30.0f));
    g.setColour (pl.text);
    g.drawText (name + " " + String (st.slice + 1), 0, 102, w - 160, 34, Justification::centredLeft);
    g.setFont (mono (10.5f));
    g.setColour (pl.dim);
    g.drawText ("OF " + String (nSlices) + "  " + String (st.src == 0 ? "A" : "B"), w - 158, 102, 84, 34, Justification::centredLeft);

    g.setFont (mono (11.0f));
    g.setColour (pl.dim);
    g.drawText ("RETRIG", 0, 172, 90, 26, Justification::centredLeft);
    g.drawText ("PITCH", 0, 236, 90, 26, Justification::centredLeft);
    g.setFont (serif (26.0f));
    g.setColour (pl.text);
    g.drawText ((st.pitch > 0 ? "+" : "") + String ((int) st.pitch), 126, 234, 58, 30, Justification::centred);
}

void Inspector::mouseDown (const juce::MouseEvent& e)
{
    if (hasStep() && waveArea().contains (e.getPosition()))
        proc.previewSlice (step().src, step().slice);
}
