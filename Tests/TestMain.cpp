// Offline tests for the J-Slice core : analysis, slicing, classification, generation, rendering.
// Builds a synthetic "amen-like" break with known ground truth, then checks what the analyser finds.

#include <juce_audio_formats/juce_audio_formats.h>
#include "Core/Analyzer.h"
#include "Core/Generator.h"
#include "Core/Exporter.h"
#include <cstdio>
#include <cmath>
#include <cstring>

using namespace bc;

namespace
{
    struct Truth { double time; HitClass cls; };

    int failures = 0;
    void check (bool ok, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }

    // ---- tiny drum synths
    void addKick (juce::AudioBuffer<float>& b, int64_t at, double sr, float gain)
    {
        double phase = 0.0;
        for (int i = 0; i < (int) (0.35 * sr) && at + i < b.getNumSamples(); ++i)
        {
            const double t = i / sr;
            const double f = 50.0 + 120.0 * std::exp (-t / 0.025);
            phase += juce::MathConstants<double>::twoPi * f / sr;
            const float v = (float) (std::sin (phase) * std::exp (-t / 0.12)) * gain;
            const float click = i < (int) (0.002 * sr) ? 0.3f * gain : 0.0f;
            for (int c = 0; c < 2; ++c) b.addSample (c, (int) (at + i), v + click);
        }
    }

    void addSnare (juce::AudioBuffer<float>& b, int64_t at, double sr, float gain, juce::Random& rng)
    {
        float lp = 0.0f;
        for (int i = 0; i < (int) (0.25 * sr) && at + i < b.getNumSamples(); ++i)
        {
            const double t = i / sr;
            const float noise = rng.nextFloat() * 2.0f - 1.0f;
            lp += 0.5f * (noise - lp);
            const float n = (noise - 0.4f * lp) * (float) std::exp (-t / 0.08);
            const float tone = (float) (std::sin (juce::MathConstants<double>::twoPi * 190.0 * t) * std::exp (-t / 0.05));
            const float v = (0.55f * n + 0.5f * tone) * gain;
            for (int c = 0; c < 2; ++c) b.addSample (c, (int) (at + i), v);
        }
    }

    void addHat (juce::AudioBuffer<float>& b, int64_t at, double sr, float gain, juce::Random& rng)
    {
        float x1 = 0.0f, x2 = 0.0f;
        for (int i = 0; i < (int) (0.06 * sr) && at + i < b.getNumSamples(); ++i)
        {
            const double t = i / sr;
            const float x = rng.nextFloat() * 2.0f - 1.0f;
            const float hp = x - 2.0f * x1 + x2;   // second-order difference = high-pass
            x2 = x1; x1 = x;
            const float v = 0.25f * hp * (float) std::exp (-t / 0.018) * gain;
            for (int c = 0; c < 2; ++c) b.addSample (c, (int) (at + i), v);
        }
    }

    juce::AudioBuffer<float> makeBreak (double sr, double bpm, int bars, std::vector<Truth>& truth,
                                       double leadIn = 0.0, double extraTail = 0.0)
    {
        juce::Random rng (1234);
        const double stepSec = 60.0 / bpm / 4.0;
        const int64_t len = (int64_t) ((bars * 16 * stepSec + leadIn + extraTail) * sr);
        juce::AudioBuffer<float> b (2, (int) len);
        b.clear();

        // amen-like bar : K . K . S . . S . S K K . . S .   (ghosts/hats in between)
        const char* bar0 = "K.K.S..S.SKK..S.";
        const char* bar1 = "K.K.S..S.S.K..SG";
        const char* hats = "h.h.h.h.h.h.h.h.";
        const char* ghst = ".g....g.......g.";

        for (int bar = 0; bar < bars; ++bar)
        {
            const char* pat = (bar % 2 == 0) ? bar0 : bar1;
            for (int s = 0; s < 16; ++s)
            {
                const double jitter = (rng.nextFloat() - 0.5f) * 0.004;
                const double t = leadIn + (bar * 16 + s) * stepSec + jitter;
                const int64_t at = juce::jmax<int64_t> (0, (int64_t) (t * sr));
                const char c = pat[s];
                bool main = false;
                if (c == 'K') { addKick (b, at, sr, 0.9f); truth.push_back ({ t, HitClass::Kick }); main = true; }
                else if (c == 'S') { addSnare (b, at, sr, 0.8f, rng); truth.push_back ({ t, HitClass::Snare }); main = true; }
                else if (c == 'G' || ghst[s] == 'g') { addSnare (b, at, sr, 0.18f, rng); truth.push_back ({ t, HitClass::Ghost }); main = true; }
                if (hats[s] == 'h')
                {
                    addHat (b, at, sr, 0.5f, rng);
                    if (! main) truth.push_back ({ t, HitClass::Hat });
                }
            }
        }
        return b;
    }

    void writeWav (const juce::AudioBuffer<float>& b, double sr, const juce::File& f)
    {
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
        if (w != nullptr) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    void analyseCase (const juce::String& name, double sr, double bpm, int bars, double leadIn, double tail)
    {
        std::printf ("\n== %s (%.0f Hz, %.1f BPM, %d bars, lead-in %.0f ms, tail %.0f ms)\n",
                     name.toRawUTF8(), sr, bpm, bars, leadIn * 1000, tail * 1000);
        std::vector<Truth> truth;
        auto audio = makeBreak (sr, bpm, bars, truth, leadIn, tail);
        auto an = Analyzer::analyze (audio, sr);
        std::printf ("  detected %.2f BPM, %d bars, confidence %.2f, origin %.1f ms, %d candidate onsets\n",
                     an.bpm, an.bars, an.confidence, an.gridOrigin * 1000.0 / sr, (int) an.onsets.size());
        check (std::abs (an.bpm - bpm) / bpm < 0.03, "tempo within 3 %");
        check (an.bars == bars || tail > 0.0, "bar count");

        for (float sens : { 0.2f, 0.5f, 0.8f })
        {
            SlicerSettings st; st.sensitivity = sens;
            auto slices = Analyzer::makeSlices (an, audio, sr, an.bpm, an.bars, an.gridOrigin, st);
            Analyzer::classify (slices, audio, sr, an.bpm, an.gridOrigin);

            int matched = 0, correctCls = 0, mainTotal = 0, mainFound = 0;
            int confusion[4][4] = {};
            for (auto& t : truth)
            {
                const bool isMain = t.cls == HitClass::Kick || t.cls == HitClass::Snare;
                if (isMain) ++mainTotal;
                for (auto& s : slices)
                {
                    if (std::abs ((double) s.start / sr - t.time) < 0.012)
                    {
                        ++matched;
                        if (isMain) ++mainFound;
                        if (s.cls == t.cls) ++correctCls;
                        ++confusion[(int) t.cls][(int) s.cls];
                        break;
                    }
                }
            }
            std::printf ("  sens %.1f : %3d slices | hits found %d/%d | kick+snare found %d/%d | class correct %d/%d\n",
                         sens, (int) slices.size(), matched, (int) truth.size(), mainFound, mainTotal, correctCls, matched);
            if (sens == 0.5f)
            {
                std::printf ("    confusion (rows = truth K S H G, cols = detected K S H G)\n");
                for (int r = 0; r < 4; ++r)
                    std::printf ("      %s : %2d %2d %2d %2d\n", hitClassLetter ((HitClass) r), confusion[r][0], confusion[r][1], confusion[r][2], confusion[r][3]);
                check (mainFound >= (int) (mainTotal * 0.95), "all main hits sliced at sensitivity 0.5");
                check (correctCls >= (int) (matched * 0.85), "classification >= 85 %");
                check ((int) slices.size() <= (int) truth.size() + 2, "no over-slicing");
            }
        }
    }
}

int main (int argc, char** argv)
{
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("test-output");
    outDir.createDirectory();

    // ---- style table sanity
    std::printf ("== Style templates\n");
    for (auto& s : getStyles())
    {
        const bool ok = std::strlen (s.kick) == 32 && std::strlen (s.snare) == 32 && std::strlen (s.ghost) == 32 && std::strlen (s.hat) == 32;
        check (ok, juce::String ("template length 32 : ") + s.name);
    }

    analyseCase ("Amen-like 4 bars", 44100.0, 136.0, 4, 0.0, 0.0);
    analyseCase ("Amen-like 2 bars @48k", 48000.0, 136.0, 2, 0.0, 0.0);
    analyseCase ("Slow break 1 bar", 44100.0, 95.0, 1, 0.0, 0.0);
    analyseCase ("Fast break, lead-in", 44100.0, 165.0, 4, 0.03, 0.0);
    analyseCase ("Untrimmed tail", 44100.0, 110.0, 4, 0.0, 0.35);

    // ---- generation + rendering
    std::printf ("\n== Generation\n");
    std::vector<Truth> truth;
    const double sr = 44100.0;
    auto audio = makeBreak (sr, 136.0, 4, truth);
    writeWav (audio, sr, outDir.getChildFile ("synthetic_break_136bpm.wav"));

    auto an = Analyzer::analyze (audio, sr);
    AudioData::Ptr ad = new AudioData();
    ad->buffer = audio; ad->sampleRate = sr; ad->name = "synthetic";
    Kit::Ptr kit = new Kit();
    kit->audio = ad; kit->bpm = an.bpm; kit->bars = an.bars; kit->gridOrigin = an.gridOrigin;
    kit->slices = Analyzer::makeSlices (an, audio, sr, an.bpm, an.bars, an.gridOrigin, {});
    Analyzer::classify (kit->slices, audio, sr, an.bpm, an.gridOrigin);

    for (int si = 0; si < (int) getStyles().size(); ++si)
    {
        const auto& style = getStyles()[(size_t) si];
        GenSettings gs = style.defaults;
        int totalOn = 0, rolls = 0, rev = 0, pitched = 0;
        bool valid = true;
        for (uint32_t seed = 1; seed <= 20; ++seed)
        {
            auto p = Generator::generate (*kit, gs, seed, nullptr);
            for (auto& st : p.steps)
            {
                if (! st.on) continue;
                ++totalOn; rolls += st.roll > 1; rev += st.reverse; pitched += st.pitch != 0;
                if (st.slice < 0 || st.slice >= (int) kit->slices.size()) valid = false;
            }
            auto p2 = Generator::generate (*kit, gs, seed, nullptr);
            if (! (p == p2)) valid = false; // deterministic for a given seed
            Generator::mutate (p, *kit, gs, seed + 99);
            for (auto& st : p.steps) if (st.on && (st.slice < 0 || st.slice >= (int) kit->slices.size())) valid = false;
        }
        std::printf ("  %-22s avg hits/8 bars %5.1f | rolls %4.1f | reversed %4.1f | pitched %4.1f\n",
                     style.name, totalOn / 20.0, rolls / 20.0, rev / 20.0, pitched / 20.0);
        check (valid, juce::String ("valid + deterministic : ") + style.name);

        // anchors respected in strict styles
        if (gs.backbone > 0.9f)
        {
            auto p = Generator::generate (*kit, gs, 7, nullptr);
            int ok = 0, total = 0;
            juce::StringArray toks = juce::StringArray::fromTokens (style.anchors, " ", "");
            for (int b = 0; b < 2; ++b)
                for (auto& t : toks)
                {
                    const int pos = t.substring (1).getIntValue();
                    if (pos / 16 != b) continue;
                    ++total;
                    auto& st = p.at (b, pos % 16);
                    const auto want = t[0] == 'K' ? HitClass::Kick : HitClass::Snare;
                    if (st.on && kit->slices[(size_t) st.slice].cls == want) ++ok;
                }
            check (ok >= total - 1, juce::String ("skeleton respected (") + juce::String (ok) + "/" + juce::String (total) + ") : " + style.name);
        }
    }

    // pattern serialisation round trip
    {
        auto p = Generator::generate (*kit, getStyles()[0].defaults, 42, nullptr);
        p.steps[3].locked = true;
        auto q = Pattern::fromString (p.toString());
        for (size_t i = 0; i < p.steps.size(); ++i)
            if (! (p.steps[i] == q.steps[i]))
                std::printf ("    step %d differs: gate %f vs %f, slice %d vs %d\n", (int) i, p.steps[i].gate, q.steps[i].gate, p.steps[i].slice, q.steps[i].slice);
        check (p == q, "pattern serialisation round trip");

        auto r = Generator::generate (*kit, getStyles()[3].defaults, 43, &p);
        check (r.steps[3] == p.steps[3], "locked step survives regeneration");
    }

    // ---- render demo loops
    std::printf ("\n== Rendering demo loops at 172 BPM\n");
    for (int si : { 0, 2, 3, 7, 9 })
    {
        const auto& style = getStyles()[(size_t) si];
        auto p = Generator::generate (*kit, style.defaults, 2026, nullptr);
        EngineParams ep;
        ep.patternBars = 4; ep.fillEvery = 4; ep.tailMode = style.tailMode; ep.tempoMode = style.tempoMode; ep.swing = style.swing;
        ep.crushOn = true; ep.bits = 12; ep.rate = 26000;
        auto f = outDir.getChildFile (juce::String ("demo_") + juce::String (style.name).replaceCharacters (" /()", "____") + ".wav");
        const bool ok = Exporter::renderAudio (kit, p, ep, 4, 172.0, 44100.0, f);

        // check the render is not silent / not clipping badly
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        float peak = 0.0f; double rms = 0.0;
        if (rd != nullptr)
        {
            juce::AudioBuffer<float> b (2, (int) rd->lengthInSamples);
            rd->read (&b, 0, b.getNumSamples(), 0, true, true);
            peak = b.getMagnitude (0, b.getNumSamples());
            rms = b.getRMSLevel (0, 0, b.getNumSamples());
        }
        std::printf ("  %-22s peak %.2f rms %.3f -> %s\n", style.name, peak, rms, f.getFileName().toRawUTF8());
        check (ok && peak > 0.05f && peak < 4.0f && rms > 0.01, juce::String ("render ok : ") + style.name);

        const bool midiOk = Exporter::writeMidi (p, ep, 4, 172.0, outDir.getChildFile ("demo.mid"));
        check (midiOk, "MIDI export");
    }

    // cyclic stretch mode render
    {
        auto p = Generator::generate (*kit, getStyles()[0].defaults, 5, nullptr);
        EngineParams ep; ep.patternBars = 2; ep.tempoMode = 2; ep.cycleMs = 35;
        const bool ok = Exporter::renderAudio (kit, p, ep, 2, 172.0, 44100.0, outDir.getChildFile ("demo_cyclic_stretch.wav"));
        check (ok, "cyclic stretch render");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
