// Integration harness (Linux, run under xvfb) : instantiates the real plugin processor + editor,
// loads two breaks, plays them through a fake host transport, checks the live macros, round-trips
// the state, drives the UI with mouse / key events and saves screenshots of every page.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

namespace
{
    struct FakePlayHead : public juce::AudioPlayHead
    {
        double ppq = 0.0, bpm = 172.0;
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo p;
            p.setBpm (bpm);
            p.setPpqPosition (ppq);
            p.setIsPlaying (playing);
            return p;
        }
    };

    int failures = 0;
    void check (bool ok, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }

    juce::MouseEvent makeEvent (juce::Component& c, juce::Point<float> pos, juce::Point<float> downPos,
                                juce::ModifierKeys mods, int clicks, bool dragged)
    {
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        return juce::MouseEvent (src, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, juce::Time::getCurrentTime(),
                                 downPos, juce::Time::getCurrentTime(), clicks, dragged);
    }

    template <typename T> T* findChild (juce::Component& root)
    {
        for (auto* c : root.getChildren())
        {
            if (auto* t = dynamic_cast<T*> (c)) return t;
            if (auto* t = findChild<T> (*c)) return t;
        }
        return nullptr;
    }

    template <typename T> void findAll (juce::Component& root, std::vector<T*>& out)
    {
        for (auto* c : root.getChildren())
        {
            if (auto* t = dynamic_cast<T*> (c)) out.push_back (t);
            findAll<T> (*c, out);
        }
    }

    juce::TextButton* findButton (juce::Component& root, const juce::String& text)
    {
        std::vector<juce::TextButton*> all;
        findAll<juce::TextButton> (root, all);
        for (auto* b : all) if (b->getButtonText() == text) return b;
        return nullptr;
    }

    void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

    void setParam (JSliceProcessor& p, const char* id, float plain)
    {
        if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
    }

    bool patternValid (JSliceProcessor& p)
    {
        auto check1 = [&p] (const bc::Step& st)
        {
            if (! st.on) return true;
            auto k = p.getKit (st.src);
            return k != nullptr && st.slice >= 0 && st.slice < (int) k->slices.size();
        };
        for (auto& st : p.getPattern().steps) if (! check1 (st)) return false;
        for (auto& st : p.getPattern().fill)  if (! check1 (st)) return false;
        return true;
    }

    int countHits (const bc::Pattern& p, int src)
    {
        int n = 0;
        for (auto& st : p.steps) n += st.on && (src < 0 || st.src == src);
        return n;
    }

    void snapshot (juce::Component& c, const juce::File& f)
    {
        auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::printf ("usage: JSliceHarness <breakA.wav> <outDir> [breakB.wav]\n"); return 2; }
    const juce::File breakFile { juce::String (argv[1]) };
    const juce::File outDir { juce::String (argv[2]) };
    juce::File breakB = argc > 3 ? juce::File (juce::String (argv[3])) : breakFile.getSiblingFile ("demo_Two-Step_DnB.wav");
    if (! breakB.existsAsFile()) breakB = breakFile;
    outDir.createDirectory();

    auto proc = std::make_unique<JSliceProcessor>();
    proc->setPlayConfigDetails (0, 2, 44100.0, 512);
    proc->prepareToPlay (44100.0, 512);
    FakePlayHead ph;
    proc->setPlayHead (&ph);

    // ---- break A alone
    juce::String err;
    check (proc->loadFile (0, breakFile, err), "load break A " + err);
    auto kit = proc->getKit (0);
    check (kit != nullptr && kit->slices.size() > 8, "slices A: " + juce::String (kit != nullptr ? (int) kit->slices.size() : 0));
    std::printf ("  break A: %.2f BPM, %d bars\n", kit->bpm, kit->bars);
    check (countHits (proc->getPattern(), 0) > 10 && countHits (proc->getPattern(), 1) == 0, "pattern with A only (" + juce::String (countHits (proc->getPattern(), 0)) + " hits)");

    // ---- break B : a second generator places B hits in A's gaps, one hit per step
    check (proc->loadFile (1, breakB, err), "load break B " + err);
    pump (100);
    auto kitB = proc->getKit (1);
    check (kitB != nullptr && kitB->slices.size() > 4, "slices B: " + juce::String (kitB != nullptr ? (int) kitB->slices.size() : 0));
    for (int mode = 0; mode < 3; ++mode)
    {
        setParam (*proc, PID::duoMode, (float) mode);
        pump (120);
        const int a = countHits (proc->getPattern(), 0), b = countHits (proc->getPattern(), 1);
        std::printf ("  duo %-16s : A %d hits, B %d hits\n", bc::duoModeNames()[mode].toRawUTF8(), a, b);
        check (a > 10 && b > 5 && patternValid (*proc), "duo mode " + bc::duoModeNames()[mode] + " : both breaks play, slices valid");
    }
    setParam (*proc, PID::duoMode, 0.0f);
    pump (120);

    // B off : the pattern goes back to A only, B on : B comes back with the same seed
    {
        const auto withB = proc->getPattern();
        setParam (*proc, PID::bOn, 0.0f);
        pump (120);
        check (countHits (proc->getPattern(), 1) == 0, "B off : no B hits");
        setParam (*proc, PID::bOn, 1.0f);
        pump (120);
        check (proc->getPattern() == withB, "B on again : same duo pattern (same seed)");
    }

    // ---- play 8 bars through processBlock
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double peak = 0.0, sumSq = 0.0; long long count = 0;
    const int blocks = (int) (8 * 4 * 60.0 / ph.bpm * 44100.0 / 512);
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (b == 50) midi.addEvent (juce::MidiMessage::noteOn (1, bc::kFirstSliceNote + 3, (juce::uint8) 100), 10);    // slice 4 of A
        if (b == 55) midi.addEvent (juce::MidiMessage::noteOn (1, bc::kFirstSliceNoteB + 2, (juce::uint8) 100), 10);   // slice 3 of B
        if (b == 60) midi.addEvent (juce::MidiMessage::noteOn (1, bc::kNoteFill, (juce::uint8) 100), 0);
        proc->processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                const float v = buf.getSample (c, i);
                if (! std::isfinite (v)) { check (false, "non-finite sample"); return 1; }
                peak = juce::jmax (peak, (double) std::abs (v));
                sumSq += v * v; ++count;
            }
        ph.ppq += 512.0 / 44100.0 * ph.bpm / 60.0;
        if (b % 40 == 0) pump (1);
    }
    std::printf ("  playback: peak %.3f, rms %.4f\n", peak, std::sqrt (sumSq / (double) count));
    check (peak > 0.05 && peak < 4.0, "audible, not exploding");

    // ---- generation via MIDI note + timer
    const auto seedBefore = proc->getPattern().seed;
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, bc::kNoteGenerate, (juce::uint8) 100), 0);
    proc->processBlock (buf, midi);
    pump (100);
    check (proc->getPattern().seed != seedBefore, "MIDI C0 generates a new pattern");

    // ---- live macros : heard while turning, same seed, hand-edited and locked steps kept
    {
        setParam (*proc, PID::complexity, 0.0f);
        setParam (*proc, PID::energy, 0.0f);
        pump (150);
        const auto base = proc->getPattern();
        proc->modifyStep (5, [] (bc::Step& s) { s = bc::Step(); s.on = true; s.slice = 1; s.vel = 77; });
        proc->modifyStep (9, [] (bc::Step& s) { s.locked = true; });
        const auto edited5 = proc->getPattern().steps[5], locked9 = proc->getPattern().steps[9];
        setParam (*proc, PID::energy, 0.8f);
        pump (150);
        const auto& now = proc->getPattern();
        check (now.seed == base.seed, "macro move keeps the seed");
        check (countHits (now, -1) > countHits (base, -1), "ENERGY up adds hits live (" + juce::String (countHits (base, -1)) + " -> " + juce::String (countHits (now, -1)) + ")");
        check (now.steps[5] == edited5 && now.steps[9] == locked9, "hand-edited and locked steps survive the macro");
        setParam (*proc, PID::energy, 0.0f);
        pump (150);
        int same = 0;
        for (size_t i = 0; i < base.steps.size(); ++i) same += proc->getPattern().steps[i] == base.steps[i];
        check (same >= 125, "ENERGY back to 0 gives the same pattern back (" + juce::String (same) + "/128 steps)");
        proc->clearLocks();
        setParam (*proc, PID::complexity, 0.5f);    // back to the centre : the style as is
        setParam (*proc, PID::energy, 0.5f);
        pump (150);
    }

    // ---- pattern slots : each slot keeps its own pattern ; the A/B breaks are shared
    {
        const auto first = proc->getPattern();
        proc->selectSlot (1);
        check (proc->getEditSlot() == 1, "slot A02 selected");
        proc->generate();
        const auto second = proc->getPattern();
        check (! (second == first) && countHits (second, 1) > 0, "A02 has its own duo pattern");
        proc->selectSlot (0);
        check (proc->getPattern().steps == first.steps, "A01 comes back unchanged");
        proc->copySlot();
        proc->selectSlot (2);
        proc->pasteSlot();
        check (proc->getPattern().steps == first.steps, "copy / paste a slot");
        proc->selectSlot (0);
    }

    // ---- styles
    for (int s = 0; s < (int) bc::getStyles().size(); ++s)
    {
        proc->applyStyle (s);
        if (countHits (proc->getPattern(), -1) == 0) check (false, juce::String ("style produced an empty pattern: ") + bc::getStyles()[(size_t) s].name);
        if (! patternValid (*proc)) check (false, juce::String ("style produced invalid slices: ") + bc::getStyles()[(size_t) s].name);
    }
    proc->applyStyle (0);
    pump (100);

    // ---- state round trip
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    std::printf ("  state size: %d KB\n", (int) state.getSize() / 1024);
    {
        auto proc2 = std::make_unique<JSliceProcessor>();
        proc2->setStateInformation (state.getData(), (int) state.getSize());
        pump (150);
        check (proc2->getPattern() == proc->getPattern(), "pattern restored (A + B)");
        check (proc2->getKit (0) != nullptr && proc2->getKit (0)->slices.size() == proc->getKit (0)->slices.size(), "slices A restored");
        check (proc2->getKit (1) != nullptr && proc2->getKit (1)->slices.size() == proc->getKit (1)->slices.size(), "slices B restored");
        check (proc2->slotUsed (1) && proc2->slotUsed (2), "pattern slots restored");
    }

    // ---- state restore without the original file (embedded FLAC)
    {
        auto tmp = outDir.getChildFile ("moved_break.wav");
        breakFile.copyFileTo (tmp);
        auto proc3 = std::make_unique<JSliceProcessor>();
        juce::String e;
        proc3->loadFile (0, tmp, e);
        juce::MemoryBlock st3;
        proc3->getStateInformation (st3);
        tmp.deleteFile();
        auto proc4 = std::make_unique<JSliceProcessor>();
        proc4->setStateInformation (st3.getData(), (int) st3.getSize());
        check (proc4->getKit (0) != nullptr && proc4->getKit (0)->length() == proc3->getKit (0)->length(), "audio restored from embedded copy when the file is gone");
    }

    // ---- exports
    auto mid = proc->exportMidi();
    auto wav = proc->exportAudio();
    check (mid.existsAsFile() && mid.getSize() > 50, "MIDI export " + mid.getFullPathName());
    check (wav.existsAsFile() && wav.getSize() > 10000, "audio export " + wav.getFullPathName());
    wav.copyFileTo (outDir.getChildFile ("exported_loop.wav"));

    // ---- editor
    {
        proc->settings().setValue ("skin", juce::var (0));
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setVisible (true);
        pump (200);
        const juce::ModifierKeys left (juce::ModifierKeys::leftButtonModifier), right (juce::ModifierKeys::rightButtonModifier);

        auto* grid = findChild<StepGrid> (*ed);
        auto* insp = findChild<Inspector> (*ed);
        check (grid != nullptr && insp != nullptr, "lanes and inspector found");
        if (grid != nullptr && insp != nullptr)
        {
            // select a hit of A so the inspector has something to show on the screenshot
            int onCol = -1, offCol = -1;
            const int first = grid->getFirstIndex();
            for (int c = 0; c < grid->getColumns(); ++c)
            {
                const auto& st = proc->getPattern().steps[(size_t) (first + c)];
                if (st.on && st.src == 0 && ! st.locked && onCol < 0) onCol = c;
                if (! st.on && offCol < 0) offCol = c;
            }
            check (onCol >= 0 && offCol >= 0, "page has an A hit and an empty step");
            if (onCol >= 0)
            {
                auto c0 = grid->cellCentre (0, onCol);
                grid->mouseDown (makeEvent (*grid, c0, c0, left, 1, false));
                grid->mouseUp (makeEvent (*grid, c0, c0, juce::ModifierKeys(), 1, false));
                pump (60);
                check (grid->getSelected() == first + onCol, "click on a hit selects it");
            }
            snapshot (*ed, outDir.getChildFile ("ui_main.png"));

            if (offCol >= 0)
            {
                // click an empty cell of lane B : a B hit appears there
                auto c1 = grid->cellCentre (1, offCol);
                grid->mouseDown (makeEvent (*grid, c1, c1, left, 1, false));
                grid->mouseUp (makeEvent (*grid, c1, c1, juce::ModifierKeys(), 1, false));
                const auto st = proc->getPattern().steps[(size_t) (first + offCol)];
                check (st.on && st.src == 1, "click on an empty B cell adds a B hit");
                // double-click deletes it
                grid->mouseDoubleClick (makeEvent (*grid, c1, c1, left, 2, false));
                check (! proc->getPattern().steps[(size_t) (first + offCol)].on, "double-click deletes the hit");
                // right-click on the empty A cell : reversed A hit
                auto c2 = grid->cellCentre (0, offCol);
                grid->mouseDown (makeEvent (*grid, c2, c2, right, 1, false));
                grid->mouseUp (makeEvent (*grid, c2, c2, juce::ModifierKeys(), 1, false));
                const auto st2 = proc->getPattern().steps[(size_t) (first + offCol)];
                check (st2.on && st2.src == 0 && st2.reverse, "right-click adds a reversed hit");
                // Delete key removes the selected hit
                grid->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
                check (! proc->getPattern().steps[(size_t) (first + offCol)].on, "Delete key removes the selected hit");
            }
            if (onCol >= 0)
            {
                const int idx = first + onCol;
                auto c0 = grid->cellCentre (0, onCol);
                proc->modifyStep (idx, [] (bc::Step& s) { s.gate = 1.0f; s.roll = 1; });
                grid->mouseDown (makeEvent (*grid, c0, c0, left, 1, false));
                grid->mouseDrag (makeEvent (*grid, c0 + juce::Point<float> (-60.0f, 0.0f), c0, left, 1, true));
                grid->mouseUp (makeEvent (*grid, c0 + juce::Point<float> (-60.0f, 0.0f), c0, juce::ModifierKeys(), 1, true));
                const float g1 = proc->getPattern().steps[(size_t) idx].gate;
                grid->mouseDown (makeEvent (*grid, c0, c0, left, 1, false));
                grid->mouseDrag (makeEvent (*grid, c0 + juce::Point<float> (40.0f, 0.0f), c0, left, 1, true));
                grid->mouseUp (makeEvent (*grid, c0 + juce::Point<float> (40.0f, 0.0f), c0, juce::ModifierKeys(), 1, true));
                const float g2 = proc->getPattern().steps[(size_t) idx].gate;
                std::printf ("  drag length : %.2f -> %.2f\n", g1, g2);
                check (g1 < 0.6f && g2 > g1, "drag left shortens, drag right lengthens");

                grid->mouseDown (makeEvent (*grid, c0, c0, left, 1, false));
                grid->mouseDrag (makeEvent (*grid, c0 + juce::Point<float> (0.0f, -45.0f), c0, left, 1, true));
                grid->mouseUp (makeEvent (*grid, c0 + juce::Point<float> (0.0f, -45.0f), c0, juce::ModifierKeys(), 1, true));
                const int r1 = proc->getPattern().steps[(size_t) idx].roll;
                std::printf ("  drag retrig : x1 -> x%d\n", r1);
                check (r1 >= 3, "drag up adds re-triggers");

                // arrow keys : slice of the selected hit
                const int n = (int) proc->getKit (0)->slices.size();
                const int s0 = proc->getPattern().steps[(size_t) idx].slice;
                grid->keyPressed (juce::KeyPress (juce::KeyPress::rightKey));
                const int s1 = proc->getPattern().steps[(size_t) idx].slice;
                grid->keyPressed (juce::KeyPress (juce::KeyPress::leftKey));
                grid->keyPressed (juce::KeyPress (juce::KeyPress::leftKey));
                const int s2 = proc->getPattern().steps[(size_t) idx].slice;
                std::printf ("  arrow keys : slice %d -> %d -> %d\n", s0 + 1, s1 + 1, s2 + 1);
                check (s1 == (s0 + 1) % n && s2 == (s0 - 1 + n) % n, "left / right arrows change the slice of the selected hit");

                // velocity lane
                auto vp = grid->veloPoint (onCol, 0.25f);
                grid->mouseDown (makeEvent (*grid, vp, vp, left, 1, false));
                grid->mouseUp (makeEvent (*grid, vp, vp, juce::ModifierKeys(), 1, false));
                const int vel = proc->getPattern().steps[(size_t) idx].vel;
                check (vel > 20 && vel < 45, "velocity lane sets the velocity (" + juce::String (vel) + ")");

                // inspector : per-step delay, retrig, move to B
                grid->select (idx);
                pump (30);
                if (auto* dp = proc->apvts.getParameter (PID::dlyOn)) dp->setValueNotifyingHost (0.0f);
                if (auto* b = findButton (*insp, "Dly")) b->triggerClick();
                pump (60);
                check ((proc->getPattern().steps[(size_t) idx].fx & bc::FxDelay) != 0, "inspector DLY marks a delay throw");
                check (proc->apvts.getRawParameterValue (PID::dlyOn)->load() > 0.5f, "marking a delay throw switches the delay on");
                if (auto* b = findButton (*insp, "6")) b->triggerClick();
                pump (60);
                check (proc->getPattern().steps[(size_t) idx].roll == 6, "inspector RETRIG 6");
                if (auto* b = findButton (*insp, "A")) b->triggerClick();
                pump (60);
                check (proc->getPattern().steps[(size_t) idx].src == 1, "inspector A/B chip moves the hit to break B");
                if (auto* b = findButton (*insp, "B")) b->triggerClick();
                pump (60);
            }

            // map : click bar 5 -> the lanes show bars 5-6
            {
                auto mp = grid->mapPoint (4);
                grid->mouseDown (makeEvent (*grid, mp, mp, left, 1, false));
                grid->mouseUp (makeEvent (*grid, mp, mp, juce::ModifierKeys(), 1, false));
                check (grid->getFirstIndex() == 64, "map click shows bars 5-6");
                grid->setZoom (4);
                pump (60);
                snapshot (*ed, outDir.getChildFile ("ui_zoom4.png"));
                grid->showFill (true);
                pump (60);
                snapshot (*grid, outDir.getChildFile ("ui_fill_page.png"));
                grid->setZoom (2);
                grid->showBar (0);
            }
            pump (100);
            snapshot (*ed, outDir.getChildFile ("ui_after_gestures.png"));
        }

        // ---- zone fader : click on the CHOPPY zone name
        {
            std::vector<ui::ZoneFader*> faders;
            findAll<ui::ZoneFader> (*ed, faders);
            check (faders.size() == 3, "three zone faders");
            if (! faders.empty())
            {
                auto* f = faders[0];
                const juce::Point<float> pt (9.0f + ((float) f->getWidth() - 18.0f) * 0.5f, 50.0f);
                f->mouseDown (makeEvent (*f, pt, pt, left, 1, false));
                f->mouseUp (makeEvent (*f, pt, pt, juce::ModifierKeys(), 1, false));
                const float v = proc->apvts.getRawParameterValue (PID::complexity)->load();
                check (std::abs (v - 0.5f) < 0.01f, "click on CHOPPY sets COMPLEXITY to the centre (" + juce::String (v, 2) + ")");
                pump (150);
            }
        }

        // ---- double-click on a slice tag of break A deletes the slice
        {
            std::vector<WaveformView*> waves;
            findAll<WaveformView> (*ed, waves);
            check (waves.size() == 2, "two waveform views");
            if (! waves.empty())
            {
                auto* wv = waves[0];
                auto k = proc->getKit (0);
                const int before = (int) k->slices.size();
                const int target = 5;
                const float areaX = 6.0f, areaW = (float) wv->getWidth() - 12.0f;
                const float x = areaX + (float) ((double) k->slices[(size_t) target].start / (double) k->length()) * areaW + 3.0f;
                const juce::Point<float> pt (x, 12.0f);
                wv->mouseDoubleClick (makeEvent (*wv, pt, pt, left, 2, false));
                const int after = (int) proc->getKit (0)->slices.size();
                std::printf ("  double-click on tag : %d -> %d slices\n", before, after);
                check (after == before - 1, "double-click on a slice tag removes the slice");
            }
        }

        // ---- live sensitivity
        {
            auto st = proc->getSlicerSettings (0);
            const int n0 = (int) proc->getKit (0)->slices.size();
            st.sensitivity = 0.1f; proc->setSlicerSettings (0, st);
            const int n1 = (int) proc->getKit (0)->slices.size();
            st.sensitivity = 0.9f; proc->setSlicerSettings (0, st);
            const int n2 = (int) proc->getKit (0)->slices.size();
            std::printf ("  live sensitivity : %d -> %d -> %d slices\n", n0, n1, n2);
            check (n1 < n2 && patternValid (*proc), "sensitivity re-slices immediately, pattern stays valid");
            st.sensitivity = 0.5f; proc->setSlicerSettings (0, st);
        }

        auto clickTab = [&] (const juce::String& name)
        {
            if (auto* b = findButton (*ed, name)) b->triggerClick();
            pump (200);
        };
        clickTab ("Playback");
        snapshot (*ed, outDir.getChildFile ("ui_playback.png"));
        clickTab ("Colour");
        snapshot (*ed, outDir.getChildFile ("ui_colour.png"));
        clickTab ("Space");
        snapshot (*ed, outDir.getChildFile ("ui_space.png"));
        clickTab ("Details");
        for (int skin = 1; skin < ui::numSkins(); ++skin)
        {
            std::vector<juce::TextButton*> all;
            findAll<juce::TextButton> (*ed, all);
            for (auto* b : all) if (b->getButtonText().startsWith ("Skin")) { b->triggerClick(); break; }
            pump (200);
            snapshot (*ed, outDir.getChildFile ("ui_skin" + juce::String (skin) + ".png"));
        }
        for (int skin = ui::skinIndex(); skin != 0; skin = ui::skinIndex())
        {
            std::vector<juce::TextButton*> all;
            findAll<juce::TextButton> (*ed, all);
            for (auto* b : all) if (b->getButtonText().startsWith ("Skin")) { b->triggerClick(); break; }
            pump (50);
        }
        clickTab ("Browse");
        snapshot (*ed, outDir.getChildFile ("ui_browser.png"));
        clickTab ("Browse");

        // empty B state
        {
            auto procE = std::make_unique<JSliceProcessor>();
            juce::String e;
            procE->loadFile (0, breakFile, e);
            std::unique_ptr<juce::AudioProcessorEditor> edE (procE->createEditor());
            edE->setVisible (true);
            pump (200);
            snapshot (*edE, outDir.getChildFile ("ui_single_break.png"));
        }
        proc->settings().setValue ("skin", juce::var (0));
        check (true, "editor opened and rendered");
    }

    proc->setPlayHead (nullptr);
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "HARNESS PASSED" : "HARNESS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
