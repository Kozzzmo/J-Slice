// Offline tests for the J-Slice core : analysis, slicing, classification, generation, rendering.
// Builds a synthetic "amen-like" break with known ground truth, then checks what the analyser finds.

#include <juce_audio_formats/juce_audio_formats.h>
#include "Core/Analyzer.h"
#include "Core/Generator.h"
#include "Core/Exporter.h"
#include <cstdio>
#include <cmath>
#include <cstring>
#include "UI/UpdateCheck.h"
extern "C" {
#include "third_party/monocypher/monocypher-ed25519.h"
}

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

    // density : at zero only one or two hits per bar; density follows the knob
    {
        for (int si = 0; si < (int) getStyles().size(); ++si)
        {
            const auto& style = getStyles()[(size_t) si];
            GenSettings gs = style.defaults;
            int maxPerBar = 0, minPerBar = 99;
            double hits[3] = {};
            const float dens[3] = { 0.0f, 0.3f, gs.density };
            for (int k = 0; k < 3; ++k)
            {
                gs.density = dens[k];
                for (uint32_t seed = 1; seed <= 10; ++seed)
                {
                    auto p = Generator::generate (*kit, gs, seed, nullptr);
                    Generator::mutate (p, *kit, gs, seed + 7);
                    for (int b = 0; b < kMaxBars; ++b)
                    {
                        int n = 0;
                        for (int i = 0; i < kStepsPerBar; ++i) n += p.at (b, i).on;
                        hits[k] += n;
                        if (k == 0) { maxPerBar = juce::jmax (maxPerBar, n); minPerBar = juce::jmin (minPerBar, n); }
                    }
                }
                hits[k] /= 10.0 * kMaxBars;
            }
            std::printf ("  %-22s hits/bar : density 0 -> %.1f, 0.3 -> %.1f, default -> %.1f\n", style.name, hits[0], hits[1], hits[2]);
            check (maxPerBar <= 2 && minPerBar >= 1, juce::String ("density 0 = 1-2 hits per bar : ") + style.name);
            check (hits[0] < hits[1] && hits[1] < hits[2], juce::String ("density increases hits : ") + style.name);
        }
    }

    // pattern change at the end of the pattern (queued slot)
    {
        Engine eng;
        eng.prepare (48000.0, 512);
        eng.setKit (0, kit);
        Pattern a, b;
        a.length = 16; b.length = 32;
        a.steps[0].on = true; a.steps[0].slice = 0;
        b.steps[0].on = true; b.steps[0].slice = 1; b.steps[16].on = true; b.steps[16].slice = 1;
        eng.setPattern (a, false);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        EngineParams prm; prm.patternSteps = 64;
        Transport t; t.playing = true; t.bpm = 120.0;
        const double ppqPerBlock = 512.0 / 48000.0 * 2.0;   // 120 BPM = 2 beats / s
        int swapAt = -1, stepAfter = -1;
        for (int blk = 0; blk < 400; ++blk)
        {
            if (blk == 40) eng.queuePattern (b);             // queued in the middle of the 2nd loop of A
            const int before = eng.swapCount.load();
            eng.process (buf, midi, t, prm);
            if (eng.swapCount.load() != before && swapAt < 0) { swapAt = blk; stepAfter = eng.uiStep.load(); }
            t.ppq += ppqPerBlock;
        }
        // A = 16 steps = 4 beats = 2 s ; blk 40 = 0.85 s into the 1st loop... the switch must land on a loop end (multiple of 4 beats)
        const double swapPpq = (swapAt + 1) * ppqPerBlock;
        const double loopEnd = std::ceil (40 * ppqPerBlock / 4.0) * 4.0;
        check (swapAt >= 0 && std::abs (swapPpq - loopEnd) < ppqPerBlock + 1e-6 && stepAfter >= 0 && stepAfter < 2,
               "queued pattern starts at the end of the playing one (from its step 1)");
        check (eng.loopIndex.load() >= 0, "loop counter runs");
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

    // ---- duo : two breaks sharing the steps
    std::printf ("\n== Duo (break A + break B)\n");
    {
        std::vector<Truth> truthB;
        auto audioB = makeBreak (sr, 112.0, 2, truthB);
        auto anB = Analyzer::analyze (audioB, sr);
        AudioData::Ptr adB = new AudioData();
        adB->buffer = audioB; adB->sampleRate = sr; adB->name = "synthetic B";
        Kit::Ptr kitB = new Kit();
        kitB->audio = adB; kitB->bpm = anB.bpm; kitB->bars = anB.bars; kitB->gridOrigin = anB.gridOrigin;
        kitB->slices = Analyzer::makeSlices (anB, audioB, sr, anB.bpm, anB.bars, anB.gridOrigin, {});
        Analyzer::classify (kitB->slices, audioB, sr, anB.bpm, anB.gridOrigin);

        const auto gs = getStyles()[0].defaults;
        for (int mode = 0; mode < 3; ++mode)
        {
            int bHits = 0, aHits = 0, aBackbone = 0, aBackboneKept = 0;
            bool valid = true;
            for (uint32_t seed = 1; seed <= 10; ++seed)
            {
                auto a = Generator::generate (*kit, gs, seed, nullptr);
                auto b = Generator::generate (*kitB, Generator::settingsForB (gs, mode, 0.5f), seed * 31u + 7u, nullptr);
                auto d = Generator::combine (a, b, mode, 0.5f, seed);
                for (size_t i = 0; i < d.steps.size(); ++i)
                {
                    const auto& st = d.steps[i];
                    if (! st.on) continue;
                    const auto& k = st.src == 0 ? *kit : *kitB;
                    if (st.slice < 0 || st.slice >= (int) k.slices.size()) valid = false;
                    (st.src == 0 ? aHits : bHits)++;
                    // a step taken from A must be A's own step, a step from B must be B's own step
                    const auto& orig = st.src == 0 ? a.steps[i] : b.steps[i];
                    if (! orig.on || orig.slice != st.slice) valid = false;
                }
                for (size_t i = 0; i < a.steps.size(); i += 4)
                {
                    const auto& st = a.steps[i];
                    if (! st.on || (st.role != HitClass::Kick && st.role != HitClass::Snare)) continue;
                    ++aBackbone;
                    aBackboneKept += d.steps[i].on && d.steps[i].src == 0;
                }
                auto d2 = Generator::combine (a, b, mode, 0.5f, seed);
                if (! (d == d2)) valid = false;
            }
            std::printf ("  %-16s A hits %5.1f | B hits %5.1f | A kick/snare on beats kept %d/%d\n",
                         duoModeNames()[mode].toRawUTF8(), aHits / 10.0, bHits / 10.0, aBackboneKept, aBackbone);
            check (valid, "duo mode " + duoModeNames()[mode] + " : every hit comes from its own break, deterministic");
            check (bHits > 20 && aHits > 20, "duo mode " + duoModeNames()[mode] + " : both breaks play");
            if (mode != 1) check (aBackboneKept >= aBackbone * 9 / 10, "duo mode " + duoModeNames()[mode] + " : A keeps its backbone");
        }

        // share 0 -> 1 gives B more room
        {
            auto a = Generator::generate (*kit, gs, 5, nullptr);
            auto b = Generator::generate (*kitB, gs, 5 * 31u + 7u, nullptr);
            int lo = 0, hi = 0;
            for (auto& st : Generator::combine (a, b, 0, 0.0f, 5).steps) lo += st.on && st.src == 1;
            for (auto& st : Generator::combine (a, b, 0, 1.0f, 5).steps) hi += st.on && st.src == 1;
            std::printf ("  B share 0 -> 1 : %d -> %d B hits\n", lo, hi);
            check (hi > lo, "B share gives break B more steps");
        }

        // mutation keeps every hit inside its own break
        {
            auto d = Generator::combine (Generator::generate (*kit, gs, 9, nullptr), Generator::generate (*kitB, gs, 9 * 31u + 7u, nullptr), 0, 0.5f, 9);
            const Kit* kits[kNumBreaks] = { kit.get(), kitB.get() };
            bool ok = true;
            for (uint32_t m = 1; m <= 20; ++m)
            {
                Generator::mutateDuo (d, kits, gs, m);
                for (auto& st : d.steps)
                    if (st.on && (st.slice < 0 || st.slice >= (int) kits[st.src]->slices.size())) ok = false;
            }
            check (ok, "duo mutation keeps valid slices for both breaks");
        }

        // MIDI export : B on its own note range
        {
            auto d = Generator::combine (Generator::generate (*kit, gs, 3, nullptr), Generator::generate (*kitB, gs, 3 * 31u + 7u, nullptr), 0, 0.5f, 3);
            EngineParams ep; ep.patternSteps = 64;
            auto f = outDir.getChildFile ("duo_render.wav");
            Exporter::renderAudio (kit, d, ep, 4, 172.0, 44100.0, f, kitB);
            check (f.getSize() > 10000, "duo render (A + B) written");
        }
    }

    // ---- live macros : same seed, moving a macro only adds / removes what crosses its threshold
    std::printf ("\n== Live macros\n");
    {
        auto gs = getStyles()[0].defaults;
        auto base = Generator::generate (*kit, gs, 77, nullptr);
        auto same = Generator::generate (*kit, gs, 77, nullptr);
        check (base == same, "same seed + same settings = same pattern");
        int prevOn = 0, monotonic = 1;
        std::vector<int> changes;
        Pattern prev = base;
        for (int i = 0; i <= 10; ++i)
        {
            auto g2 = gs;
            g2.density = juce::jlimit (0.0f, 1.0f, gs.density * 0.5f + 0.05f * (float) i);
            auto p = Generator::generate (*kit, g2, 77, nullptr);
            int on = 0, diff = 0;
            for (size_t k = 0; k < p.steps.size(); ++k) { on += p.steps[k].on; diff += ! (p.steps[k] == prev.steps[k]); }
            if (i > 0) { changes.push_back (diff); if (on + 2 < prevOn) monotonic = 0; }
            prevOn = on;
            prev = p;
        }
        int maxChange = 0; for (int c : changes) maxChange = juce::jmax (maxChange, c);
        std::printf ("  density sweep : biggest change between two neighbour settings = %d steps / 128\n", maxChange);
        check (monotonic == 1, "more density never removes hits (within 2)");
        check (maxChange <= 40, "a small macro move changes a part of the pattern, not all of it");
    }

    // ---- render demo loops
    std::printf ("\n== Rendering demo loops at 172 BPM\n");
    for (int si : { 0, 2, 3, 7, 9 })
    {
        const auto& style = getStyles()[(size_t) si];
        auto p = Generator::generate (*kit, style.defaults, 2026, nullptr);
        EngineParams ep;
        ep.patternSteps = 64; ep.fillEvery = 4; ep.tailMode = style.tailMode; ep.tempoMode = style.tempoMode; ep.swing = style.swing;
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

    // stretch modes + every effect on, odd pattern length, 8x rolls
    for (int mode : { (int) TempoCyclic, (int) TempoStretch })
    {
        auto p = Generator::generate (*kit, getStyles()[0].defaults, 5, nullptr);
        p.steps[4].roll = 8;
        EngineParams ep; ep.patternSteps = 27; ep.tempoMode = mode; ep.cycleMs = 35;
        ep.compOn = ep.dlyOn = ep.revOn = ep.crushOn = ep.filterOn = ep.driveOn = true;
        ep.cutoff = 6000.0f; ep.dlySrc = 1; ep.revSrc = 1;
        auto f = outDir.getChildFile (mode == TempoCyclic ? "demo_cyclic_stretch.wav" : "demo_clean_stretch_fx.wav");
        const bool ok = Exporter::renderAudio (kit, p, ep, 2, 172.0, 44100.0, f);
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        float peak = 0.0f; bool finite = true;
        if (rd != nullptr)
        {
            juce::AudioBuffer<float> b (2, (int) rd->lengthInSamples);
            rd->read (&b, 0, b.getNumSamples(), 0, true, true);
            peak = b.getMagnitude (0, b.getNumSamples());
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < b.getNumSamples(); ++i)
                    finite &= std::isfinite (b.getSample (c, i));
        }
        std::printf ("  mode %d with all FX : peak %.2f\n", mode, peak);
        check (ok && finite && peak > 0.05f && peak < 4.0f, juce::String ("stretch mode ") + juce::String (mode) + " + FX render");
    }

    // per-step effects : a delay "throw" on one hit, lo-fi and filter on single hits
    {
        auto render = [&] (uint8_t fx, const juce::String& name) -> juce::AudioBuffer<float>
        {
            Pattern p;
            int snare = 0;
            for (int i = 0; i < (int) kit->slices.size(); ++i) if (kit->slices[(size_t) i].cls == HitClass::Snare) { snare = i; break; }
            p.steps[0].on = true; p.steps[0].slice = (int16_t) snare; p.steps[0].vel = 127; p.steps[0].fx = fx;
            EngineParams ep; ep.patternSteps = 32; ep.tailMode = 1; ep.dlyOn = true; ep.dlySrc = 2; ep.revOn = true; ep.revSrc = 2;
            ep.dlyTime = 4; ep.dlyFb = 0.5f; ep.dlyMix = 0.6f; ep.revMix = 0.9f; ep.revSize = 0.85f;
            auto f = outDir.getChildFile ("stepfx_" + name + ".wav");
            Exporter::renderAudio (kit, p, ep, 2, 172.0, 44100.0, f);
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
            juce::AudioBuffer<float> b (2, (int) rd->lengthInSamples);
            rd->read (&b, 0, b.getNumSamples(), 0, true, true);
            return b;
        };
        auto tailRms = [] (const juce::AudioBuffer<float>& b)
        {
            const int start = b.getNumSamples() / 4, len = b.getNumSamples() / 4;   // 2nd half of bar 1 : the hit itself is over
            return b.getRMSLevel (0, start, len);
        };
        const auto dry = render (0, "none");
        const auto dly = render (FxDelay, "delay");
        const auto rev = render (FxReverb, "reverb");
        const auto lofi = render (FxLofi, "lofi");
        const auto filt = render (FxFilter, "filter");
        std::printf ("  tail rms : dry %.4f, delay throw %.4f, reverb throw %.4f\n", tailRms (dry), tailRms (dly), tailRms (rev));
        check (tailRms (dly) > tailRms (dry) * 4.0f + 0.002f, "per-step delay throw");
        check (tailRms (rev) > tailRms (dry) * 4.0f + 0.0008f, "per-step reverb throw");
        auto diff = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
        {
            double d = 0.0;
            for (int i = 0; i < juce::jmin (a.getNumSamples(), b.getNumSamples()); ++i) d += std::abs (a.getSample (0, i) - b.getSample (0, i));
            return d;
        };
        check (diff (dry, lofi) > 1.0 && diff (dry, filt) > 1.0, "per-step lo-fi and filter change the hit");
        auto p2 = Pattern::fromString (Pattern().toString());
        Pattern q; q.steps[5].on = true; q.steps[5].fx = FxDelay | FxFilter;
        check (Pattern::fromString (q.toString()) == q, "per-step effects saved with the pattern");
        juce::ignoreUnused (p2);
    }

    // clean stretch keeps the break length in time : a 1-bar chunk at 172 BPM must last one bar
    {
        Pattern p;
        p.steps[0].on = true; p.steps[0].slice = 0; p.steps[0].vel = 127;
        EngineParams ep; ep.patternSteps = 16; ep.tempoMode = TempoStretch; ep.tailMode = 0;
        auto f = outDir.getChildFile ("stretch_length.wav");
        Exporter::renderAudio (kit, p, ep, 1, 172.0, 44100.0, f);
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        juce::AudioBuffer<float> b (2, (int) rd->lengthInSamples);
        rd->read (&b, 0, b.getNumSamples(), 0, true, true);
        // energy in the last 16th of the bar should still be present (the break is stretched, not cut)
        const int lastStep = b.getNumSamples() / 16;
        const float rmsEnd = b.getRMSLevel (0, b.getNumSamples() - lastStep, lastStep);
        check (rmsEnd > 0.005f, "clean stretch fills the whole bar");
        f.deleteFile();
    }

    // update manifest: signature, product, tampering, version order
    {
        uint8_t seed[32], sk[64], pk[32], sig[64];
        for (int i = 0; i < 32; ++i) seed[i] = (uint8_t) (100 + i);
        crypto_ed25519_key_pair (sk, pk, seed);
        auto sign = [&] (const std::string& body)
        {
            crypto_ed25519_sign (sig, sk, (const uint8_t*) body.data(), body.size());
            return body + "signature: " + juce::String::toHexString (sig, 64, 0).toStdString() + "\n";
        };
        const auto good = sign ("KINETIK-UPDATE 1\nproduct: J-Slice\nversion: 1.2\nnotes: new\npage: https://github.com/Kozzzmo/J-Slice/releases/tag/latest\n");
        const auto m = upd::parse (good, "J-Slice", pk);
        check (m.valid && m.version == "1.2" && m.notes == "new", "update manifest accepted");
        check (! upd::parse (good, "KDSP-16", pk).valid, "update manifest for another product refused");
        auto bad = good; bad.replace (bad.find ("1.2"), 3, "9.9");
        check (! upd::parse (bad, "J-Slice", pk).valid, "tampered update manifest refused");
        check (! upd::parse (good, "J-Slice").valid, "update manifest signed by another key refused");
        const auto evil = upd::parse (sign ("KINETIK-UPDATE 1\nproduct: J-Slice\nversion: 2\npage: https://evil.example/\n"), "J-Slice", pk);
        check (evil.valid && evil.page.rfind ("https://github.com/Kozzzmo/", 0) == 0, "update page outside github.com/Kozzzmo replaced");
        check (upd::newer ("1.10", "1.9") && upd::newer ("1.1", "1.0.9") && ! upd::newer ("1.1.0", "1.1"), "update version order");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
