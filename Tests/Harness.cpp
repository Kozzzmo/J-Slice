// Integration harness (Linux, run under xvfb) : instantiates the real plugin processor + editor,
// loads a break, plays it through a fake host transport, round-trips the state and saves
// screenshots of every UI page.

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
    if (argc < 3) { std::printf ("usage: JSliceHarness <break.wav> <outDir>\n"); return 2; }
    const juce::File breakFile { juce::String (argv[1]) };
    const juce::File outDir { juce::String (argv[2]) };
    outDir.createDirectory();

    auto proc = std::make_unique<JSliceProcessor>();
    proc->setPlayConfigDetails (0, 2, 44100.0, 512);
    proc->prepareToPlay (44100.0, 512);
    FakePlayHead ph;
    proc->setPlayHead (&ph);

    juce::String err;
    check (proc->loadFile (breakFile, err), "load break " + err);
    auto kit = proc->getKit();
    check (kit != nullptr && kit->slices.size() > 8, "slices: " + juce::String (kit != nullptr ? (int) kit->slices.size() : 0));
    std::printf ("  break: %.2f BPM, %d bars\n", kit->bpm, kit->bars);

    int on = 0;
    for (auto& s : proc->getPattern().steps) on += s.on;
    check (on > 10, "pattern generated (" + juce::String (on) + " hits)");

    // play 8 bars through processBlock
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double peak = 0.0, sumSq = 0.0; long long count = 0;
    const int blocks = (int) (8 * 4 * 60.0 / ph.bpm * 44100.0 / 512);
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (b == 50) midi.addEvent (juce::MidiMessage::noteOn (1, 36 + 3, (juce::uint8) 100), 10);   // manual slice
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
        if (b % 40 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
    }
    std::printf ("  playback: peak %.3f, rms %.4f\n", peak, std::sqrt (sumSq / (double) count));
    check (peak > 0.05 && peak < 4.0, "audible, not exploding");

    // generation via MIDI note + timer
    const auto seedBefore = proc->getPattern().seed;
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, bc::kNoteGenerate, (juce::uint8) 100), 0);
    proc->processBlock (buf, midi);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    check (proc->getPattern().seed != seedBefore, "MIDI C0 generates a new pattern");

    // styles
    for (int s = 0; s < (int) bc::getStyles().size(); ++s)
    {
        proc->applyStyle (s);
        int n = 0; for (auto& st : proc->getPattern().steps) n += st.on;
        if (n == 0) check (false, juce::String ("style produced an empty pattern: ") + bc::getStyles()[(size_t) s].name);
    }
    proc->applyStyle (0);

    // state round trip
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    std::printf ("  state size: %d KB\n", (int) state.getSize() / 1024);
    auto proc2 = std::make_unique<JSliceProcessor>();
    proc2->setStateInformation (state.getData(), (int) state.getSize());
    check (proc2->getPattern() == proc->getPattern(), "pattern restored");
    check (proc2->getKit() != nullptr && proc2->getKit()->slices.size() == proc->getKit()->slices.size(), "slices restored");

    // state restore without the original file (embedded FLAC)
    {
        auto tmp = outDir.getChildFile ("moved_break.wav");
        breakFile.copyFileTo (tmp);
        auto proc3 = std::make_unique<JSliceProcessor>();
        juce::String e;
        proc3->loadFile (tmp, e);
        juce::MemoryBlock st3;
        proc3->getStateInformation (st3);
        tmp.deleteFile();
        auto proc4 = std::make_unique<JSliceProcessor>();
        proc4->setStateInformation (st3.getData(), (int) st3.getSize());
        check (proc4->getKit() != nullptr && proc4->getKit()->length() == proc3->getKit()->length(), "audio restored from embedded copy when the file is gone");
    }

    // exports
    auto mid = proc->exportMidi();
    auto wav = proc->exportAudio();
    check (mid.existsAsFile() && mid.getSize() > 50, "MIDI export " + mid.getFullPathName());
    check (wav.existsAsFile() && wav.getSize() > 10000, "audio export " + wav.getFullPathName());
    wav.copyFileTo (outDir.getChildFile ("exported_loop.wav"));

    // editor screenshots
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setVisible (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        snapshot (*ed, outDir.getChildFile ("ui_main.png"));

        auto clickTab = [&] (const juce::String& name)
        {
            for (auto* c : ed->getChildren())
                if (auto* b = dynamic_cast<juce::TextButton*> (c))
                    if (b->getButtonText() == name) { b->triggerClick(); break; }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        };
        clickTab ("GENERATOR");
        snapshot (*ed, outDir.getChildFile ("ui_generator.png"));
        clickTab ("SOUND");
        snapshot (*ed, outDir.getChildFile ("ui_sound.png"));
        clickTab ("BROWSE");
        snapshot (*ed, outDir.getChildFile ("ui_browser.png"));
        check (true, "editor opened and rendered");
    }

    proc->setPlayHead (nullptr);
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "HARNESS PASSED" : "HARNESS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
