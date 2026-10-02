#include "Generator.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace bc
{

//==============================================================================
// Style presets. Templates : 2 bars of 16ths. '.' = never, '1'..'9' = 10..90 %, 'X' = always.
const std::vector<StyleDef>& getStyles()
{
    static const std::vector<StyleDef> styles = []
    {
        std::vector<StyleDef> v;
        //                    dens  back  ghost hats  chop  cut16 rolls stut  rev   pitP  pitR gate  var   fill  rep hum
        v.push_back ({ "Jungle 94 (Amen)",
            "X.6.......X.5..." "X.5.......X3....",
            "....X..4.4..X..3" "....X..4.4..X.5.",
            "..3..3.....3.3.." "..3..3.....3..3.",
            "..4...4...4...4." "..4...4...4...4.",
            "K0 S4 S12 K16 S20 S28",
            { 0, 0.70f, 0.55f, 0.50f, 0.20f, 0.55f, 0.20f, 0.30f, 0.25f, 0.15f, 0.12f, 3, 0.12f, 0.45f, 0.60f, 2, 0.30f },
            0, 0, 0.0f });
        v.push_back ({ "Ragga Jungle",
            "X.........X....." "X.5.......X.....",
            "....X.....5.X..." "....X..5....X.4.",
            "...3...3...3...3" "...3...3...3...3",
            "..3...3...3...3." "..3...3...3...3.",
            "K0 S4 K10 S12 K16 S20 K26 S28",
            { 1, 0.70f, 0.70f, 0.45f, 0.25f, 0.35f, 0.10f, 0.20f, 0.15f, 0.12f, 0.25f, 5, 0.10f, 0.40f, 0.55f, 2, 0.30f },
            0, 0, 0.0f });
        v.push_back ({ "Rinse-Out Choppage",
            "X.5...4...X.4..." "X..5....5.X.....",
            "....X..5.5..X.5." "..4.X..5.5..X555",
            "..4..4..4..4..4." "..4..4..4..4..4.",
            "................" "................",
            "S4 S12 S20 S28",
            { 2, 0.80f, 0.40f, 0.50f, 0.10f, 0.85f, 0.45f, 0.55f, 0.45f, 0.25f, 0.20f, 4, 0.25f, 0.60f, 0.80f, 3, 0.30f },
            0, 0, 0.0f });
        v.push_back ({ "Two-Step DnB",
            "X.........X....." "X.........X..3..",
            "....X.......X..." "....X.......X...",
            ".......3.3....3." "..3....3.3.....3",
            "..6...6...6...6." "..6...6...6...6.",
            "K0 S4 K10 S12 K16 S20 K26 S28",
            { 3, 0.80f, 0.95f, 0.45f, 0.45f, 0.12f, 0.05f, 0.10f, 0.05f, 0.06f, 0.04f, 2, 0.10f, 0.30f, 0.50f, 2, 0.25f },
            1, 1, 0.0f });
        v.push_back ({ "Rollers",
            "X.........X....." "X.....3...X.....",
            "....X.......X..." "....X.......X...",
            "..3..4.3.3..3..4" "..3..4.3.3.4..4.",
            "4.6.4.6.4.6.4.6." "4.6.4.6.4.6.4.6.",
            "K0 S4 K10 S12 K16 S20 K26 S28",
            { 4, 0.85f, 0.90f, 0.60f, 0.65f, 0.20f, 0.10f, 0.15f, 0.08f, 0.08f, 0.05f, 2, 0.15f, 0.35f, 0.50f, 2, 0.30f },
            1, 1, 0.10f });
        v.push_back ({ "Liquid",
            "X.........X....." "X.....4...X.....",
            "....X.......X..." "....X.......X...",
            "...3...3.3....3." ".3.3...3....3..3",
            "..5...5...5...5." "..5...5...5...5.",
            "K0 S4 K10 S12 K16 S20 K26 S28",
            { 5, 0.70f, 0.85f, 0.50f, 0.50f, 0.15f, 0.05f, 0.08f, 0.03f, 0.05f, 0.03f, 2, 0.05f, 0.30f, 0.40f, 2, 0.40f },
            1, 1, 0.18f });
        v.push_back ({ "Neurofunk / Techstep",
            "X......X........" "X......X..5.....",
            "....X.......X..." "....X.......X.4.",
            "..........3....." "......3.......3.",
            "..4...4...4...4." "..4...4...4...4.",
            "K0 S4 K7 S12 K16 S20 K23 S28",
            { 6, 0.75f, 0.97f, 0.20f, 0.30f, 0.15f, 0.25f, 0.12f, 0.10f, 0.05f, 0.05f, 2, 0.45f, 0.30f, 0.50f, 2, 0.10f },
            1, 1, 0.0f });
        v.push_back ({ "Halftime",
            "X.....5...4....." "X..4......5.....",
            "........X......." "........X.....4.",
            "....3......3...3" "....3......3...3",
            "..4...4...4...4." "..4...4...4...4.",
            "K0 S8 K16 S24",
            { 7, 0.65f, 0.95f, 0.30f, 0.40f, 0.12f, 0.15f, 0.15f, 0.10f, 0.10f, 0.05f, 3, 0.25f, 0.30f, 0.50f, 2, 0.25f },
            1, 1, 0.0f });
        v.push_back ({ "Drumfunk",
            "X..4....5.X..4.." "X.4.....4.X....4",
            "....X..5.5..X..5" "....X.5..5..X.5.",
            ".5.55.5.5.5.5.55" ".5.5.55.5.55.5.5",
            ".4...4...4...4.." ".4...4...4...4..",
            "S4 S12 S20 S28",
            { 8, 0.85f, 0.50f, 0.85f, 0.30f, 0.45f, 0.30f, 0.35f, 0.15f, 0.10f, 0.08f, 2, 0.10f, 0.55f, 0.60f, 2, 0.45f },
            0, 0, 0.12f });
        v.push_back ({ "Breakcore",
            "X.5.5.5.X.5.5.5." "X5.5X5.5X.5.X5.5",
            "....X..5..5.X.55" "..5.X.5..55.X555",
            "..4...4...4...4." "..4...4...4...4.",
            "4.4.4.4.4.4.4.4." "4.4.4.4.4.4.4.4.",
            "K0 S4 S12 K16 S20 S28",
            { 9, 0.90f, 0.40f, 0.40f, 0.20f, 1.00f, 0.70f, 0.80f, 0.70f, 0.40f, 0.50f, 12, 0.35f, 0.80f, 1.00f, 4, 0.30f },
            0, 0, 0.0f });
        return v;
    }();
    return styles;
}

juce::StringArray getStyleNames()
{
    juce::StringArray s;
    for (auto& st : getStyles()) s.add (st.name);
    return s;
}

//==============================================================================
namespace
{
    float templateValue (const char* t, int pos)
    {
        const auto len = (int) std::strlen (t);
        if (len == 0) return 0.0f;
        const char c = t[pos % len];
        if (c == 'X' || c == 'x') return 1.0f;
        if (c >= '1' && c <= '9') return (float) (c - '0') * 0.1f;
        return 0.0f;
    }

    struct Anchor { int pos; HitClass cls; };

    std::vector<Anchor> parseAnchors (const char* s)
    {
        std::vector<Anchor> out;
        for (auto& tok : juce::StringArray::fromTokens (s, " ", ""))
        {
            if (tok.length() < 2) continue;
            const auto c = tok[0];
            out.push_back ({ tok.substring (1).getIntValue(), c == 'K' ? HitClass::Kick : (c == 'S' ? HitClass::Snare : (c == 'H' ? HitClass::Hat : HitClass::Ghost)) });
        }
        return out;
    }

    struct Ctx
    {
        const Kit& kit;
        GenSettings s;
        const StyleDef& style;
        juce::Random rng;
        std::array<std::vector<int>, kNumClasses> byClass;   // sorted by quality (best first)
        std::vector<int> sliceAtStep;                         // source 16th -> slice (or -1)
        std::vector<int> goodStarts;                          // source steps that start with a strong hit
        std::vector<Anchor> anchors;
        int mainKick = -1, mainSnare = -1, altSnare = -1;
        int srcSteps = 16;

        Ctx (const Kit& k, const GenSettings& gs, uint32_t seed)
            : kit (k), s (gs), style (getStyles()[(size_t) juce::jlimit (0, (int) getStyles().size() - 1, gs.style)]),
              rng ((juce::int64) seed * 7919 + 17)
        {
            srcSteps = kit.sourceSteps();
            anchors = parseAnchors (style.anchors);
            const int n = (int) kit.slices.size();

            float maxDb = -120.0f;
            for (auto& sl : kit.slices) maxDb = juce::jmax (maxDb, sl.levelDb);
            auto quality = [&] (int i)
            {
                auto& sl = kit.slices[(size_t) i];
                return 0.5f * sl.strength + juce::jlimit (0.0f, 1.0f, (sl.levelDb - maxDb + 30.0f) / 30.0f);
            };

            for (int i = 0; i < n; ++i)
                byClass[(size_t) kit.slices[(size_t) i].cls].push_back (i);
            for (auto& list : byClass)
                std::sort (list.begin(), list.end(), [&] (int a, int b) { return quality (a) > quality (b); });

            sliceAtStep.assign ((size_t) srcSteps, -1);
            for (int i = 0; i < n; ++i)
            {
                const float g = kit.slices[(size_t) i].gridPos;
                const int cell = (int) std::round (g);
                if (std::abs (g - (float) cell) > 0.35f || cell < 0 || cell >= srcSteps) continue;
                auto& cur = sliceAtStep[(size_t) cell];
                if (cur < 0 || quality (i) > quality (cur)) cur = i;
            }
            for (int st = 0; st < srcSteps; ++st)
            {
                const int si = sliceAtStep[(size_t) st];
                if (si < 0) continue;
                const auto c = kit.slices[(size_t) si].cls;
                const int w = (c == HitClass::Kick || c == HitClass::Snare) ? 3 : 1;
                for (int rep = 0; rep < w; ++rep) goodStarts.push_back (st);
            }

            // main kick / snare : prefer strong hits sitting on the beat in the original break
            auto pickMain = [&] (HitClass c) -> int
            {
                auto& list = byClass[(size_t) c];
                if (list.empty()) return -1;
                const int topN = juce::jmin ((int) list.size(), 1 + (int) std::round (gs.variation * 2.0f));
                return list[(size_t) rng.nextInt (topN)];
            };
            mainKick = pickMain (HitClass::Kick);
            mainSnare = pickMain (HitClass::Snare);
            auto& sn = byClass[(size_t) HitClass::Snare];
            altSnare = sn.size() > 1 ? sn[(size_t) rng.nextInt ((int) juce::jmin<size_t> (sn.size(), 3))] : mainSnare;
        }

        bool chance (float p) { return rng.nextFloat() < p; }

        HitClass clsOf (int slice) const { return kit.slices[(size_t) slice].cls; }

        int pick (HitClass c)
        {
            switch (c)
            {
                case HitClass::Kick:
                    if (mainKick >= 0 && ! chance (s.variation * 0.3f + s.pitchProb * 0.2f)) return mainKick;
                    break;
                case HitClass::Snare:
                    if (mainSnare >= 0) return chance (0.8f) ? mainSnare : altSnare;
                    break;
                case HitClass::Ghost:
                    if (byClass[(size_t) HitClass::Ghost].empty())
                        return mainSnare >= 0 ? (chance (0.6f) ? altSnare : mainSnare) : -1;
                    break;
                case HitClass::Hat:
                    break;
            }
            auto& list = byClass[(size_t) c];
            if (list.empty())
            {
                if (c == HitClass::Hat) return -1;
                // fallback : any slice
                return kit.slices.empty() ? -1 : rng.nextInt ((int) kit.slices.size());
            }
            const int topN = juce::jmin ((int) list.size(), 2 + (int) std::round (s.variation * 3.0f));
            return list[(size_t) rng.nextInt (topN)];
        }

        int goodStart()
        {
            if (goodStarts.empty()) return 0;
            return goodStarts[(size_t) rng.nextInt ((int) goodStarts.size())];
        }

        int containingSlice (int srcStep) const
        {
            int best = -1;
            for (int i = 0; i < (int) kit.slices.size(); ++i)
                if (kit.slices[(size_t) i].gridPos <= (float) srcStep + 0.35f) best = i;
            return best < 0 ? (kit.slices.empty() ? -1 : 0) : best;
        }

        bool isAnchor (int t32) const
        {
            for (auto& a : anchors) if (a.pos == t32) return true;
            return false;
        }

        uint8_t vel (int base, int spread = 10)
        {
            const int h = (int) std::round (s.humanize * (float) spread);
            const int v = base + (h > 0 ? rng.nextInt (2 * h + 1) - h : 0);
            return (uint8_t) juce::jlimit (1, 127, v);
        }

        Step makeStep (int slice, HitClass role, int velBase)
        {
            Step st;
            st.on = slice >= 0;
            st.slice = (int16_t) juce::jmax (0, slice);
            st.role = role;
            st.vel = vel (velBase);
            return st;
        }

        int baseVelFor (HitClass c) const
        {
            switch (c)
            {
                case HitClass::Kick:  return 112;
                case HitClass::Snare: return 112;
                case HitClass::Hat:   return 90;
                case HitClass::Ghost: return 100;
            }
            return 100;
        }
    };

    //==========================================================================
    // Phrase generation : Collins-style cut partition over the break, then the style skeleton,
    // then the template hits, then ornaments.
    void genPhrase (Ctx& c, Step* out, int firstAbsStep, int numSteps)
    {
        const int P = numSteps;
        std::vector<float> srcPos ((size_t) P, -1.0f);
        std::vector<char> trig ((size_t) P, 0), stutter ((size_t) P, 0);

        // ---- 1. cut partition (odd-sized blocks, repeats, stutter endings)
        const int unit = c.chance (c.s.cut16) ? 1 : 2;
        int pos = 0;
        while (pos < P)
        {
            const int left = P - pos;
            if (left <= 8 && left >= 2 && c.chance (c.s.stutter * 0.5f))
            {
                const int len = (left >= 4 && c.chance (0.5f)) ? 2 : 1;
                const int offset = c.goodStart();
                for (int i = 0; i < left; ++i)
                {
                    srcPos[(size_t) (pos + i)] = (float) ((offset + (i % len)) % c.srcSteps);
                    trig[(size_t) (pos + i)] = (i % len) == 0;
                    stutter[(size_t) (pos + i)] = 1;
                }
                break;
            }

            const float r = c.rng.nextFloat();
            int lenU = r < 0.25f ? 1 : (r < 0.85f ? 3 : (c.s.chop > 0.5f ? 5 : 3));
            int lenSteps = lenU * unit;
            while (lenSteps > left && lenU > 1) { lenU -= 2; lenSteps = lenU * unit; }
            if (lenSteps > left) lenSteps = left;

            const bool jump = c.chance (c.s.chop);
            int reps = jump || c.chance (c.s.chop * 0.5f) ? 1 + c.rng.nextInt (juce::jmax (1, c.s.maxRepeats)) : 1;
            if (reps * lenSteps > left) { lenSteps = left; reps = 1; }

            const int absStep = firstAbsStep + pos;
            const int offset = jump ? c.goodStart() : (absStep % c.srcSteps);
            for (int rep = 0; rep < reps; ++rep)
            {
                for (int i = 0; i < lenSteps; ++i)
                    srcPos[(size_t) (pos + i)] = (float) ((offset + i) % c.srcSteps);
                if (jump || rep > 0) trig[(size_t) pos] = 1;
                pos += lenSteps;
            }
        }

        // ---- 2. events following the (re-arranged) break
        for (int i = 0; i < P; ++i)
        {
            Step st;
            const int sp = (int) srcPos[(size_t) i];
            if (sp >= 0)
            {
                int sl = c.sliceAtStep[(size_t) sp];
                if (sl < 0 && trig[(size_t) i]) sl = c.containingSlice (sp);
                if (sl >= 0)
                {
                    const auto cls = c.clsOf (sl);
                    st = c.makeStep (sl, cls, c.baseVelFor (cls));
                    if (stutter[(size_t) i] && c.chance (c.s.rolls)) st.roll = 2;
                }
            }
            out[i] = st;
        }

        // ---- 3. style skeleton (backbone) and template hits
        for (int i = 0; i < P; ++i)
        {
            const int t32 = (firstAbsStep + i) % 32;
            auto& st = out[i];

            bool anchored = false;
            for (auto& a : c.anchors)
            {
                if (a.pos != t32) continue;
                anchored = true;
                if (! c.chance (c.s.backbone)) break;
                if (! (st.on && c.clsOf (st.slice) == a.cls))
                {
                    const int sl = c.pick (a.cls);
                    if (sl >= 0) st = c.makeStep (sl, a.cls, 120);
                }
                else st.vel = c.vel (118);
                st.roll = 1;
                break;
            }
            if (anchored) continue;

            // strong style : source kicks/snares outside the template become ghosts or disappear
            if (st.on && c.s.backbone > 0.5f)
            {
                const auto cls = c.clsOf (st.slice);
                const float tv = cls == HitClass::Kick ? templateValue (c.style.kick, t32)
                               : cls == HitClass::Snare ? templateValue (c.style.snare, t32) : 1.0f;
                if (tv < 0.05f && c.chance ((c.s.backbone - 0.5f) * 1.8f))
                {
                    if (cls == HitClass::Snare && c.chance (c.s.ghosts + 0.2f))
                    {
                        st.role = HitClass::Ghost;
                        st.vel = c.vel (45, 12);
                    }
                    else st = Step();
                }
            }

            if (! st.on)
            {
                const float pk = templateValue (c.style.kick, t32) * c.s.density;
                const float ps = templateValue (c.style.snare, t32) * c.s.density;
                const float pg = templateValue (c.style.ghost, t32) * c.s.ghosts;
                const float ph = templateValue (c.style.hat, t32) * c.s.hats;
                const float r = c.rng.nextFloat();
                if (r < pk)                          { const int sl = c.pick (HitClass::Kick);  if (sl >= 0) st = c.makeStep (sl, HitClass::Kick, 104); }
                else if (r < pk + ps)                { const int sl = c.pick (HitClass::Snare); if (sl >= 0) st = c.makeStep (sl, HitClass::Snare, 106); }
                else if (r < pk + ps + pg)           { const int sl = c.pick (HitClass::Ghost); if (sl >= 0) st = c.makeStep (sl, HitClass::Ghost, 48); }
                else if (r < juce::jmin (1.0f, pk + ps + pg + ph)) { const int sl = c.pick (HitClass::Hat); if (sl >= 0) st = c.makeStep (sl, HitClass::Hat, 82); }
            }
            else if (! trig[(size_t) i] && c.chance ((1.0f - c.s.density) * 0.35f))
            {
                st = Step(); // thinning
            }
        }

        // ---- 4. ornaments
        for (int i = 0; i < P; ++i)
        {
            auto& st = out[i];
            if (! st.on) continue;
            const int t32 = (firstAbsStep + i) % 32;
            const int inBar = t32 % 16;
            const bool anchor = c.isAnchor (t32);
            const auto cls = st.role;

            // rolls : mostly snares / ghosts, more likely towards the end of the bar
            if (st.roll == 1 && ! anchor && (cls == HitClass::Snare || cls == HitClass::Ghost || c.chance (c.s.stutter * 0.3f)))
            {
                const float w = inBar >= 12 ? 1.0f : (inBar % 2 == 1 ? 0.6f : 0.35f);
                if (c.chance (c.s.rolls * w))
                {
                    const float r = c.rng.nextFloat();
                    st.roll = r < 0.6f ? 2 : (r < 0.75f ? 3 : 4);
                }
            }

            // reversed snare leading into the backbeat snare
            if (anchor && cls == HitClass::Snare && i > 0 && ! c.isAnchor ((firstAbsStep + i - 1) % 32) && c.chance (c.s.reverse))
            {
                auto& prev = out[i - 1];
                prev = st;
                prev.reverse = true;
                prev.vel = c.vel (96);
                prev.roll = 1;
            }
            else if (! anchor && c.chance (c.s.reverse * 0.2f))
                st.reverse = true;

            if (c.chance (c.s.pitchProb) && c.s.pitchRange > 0)
            {
                const int amt = 1 + c.rng.nextInt (c.s.pitchRange);
                st.pitch = (int8_t) (c.chance (0.65f) ? amt : -amt);
            }

            if (! anchor && c.chance (c.s.gateProb))
            {
                const float g[] = { 0.3f, 0.5f, 0.7f };
                st.gate = g[c.rng.nextInt (3)];
            }
        }
    }

    //==========================================================================
    void mutateSteps (Ctx& c, Step* bar, int firstAbsStep, int numSteps, int numOps)
    {
        for (int op = 0; op < numOps; ++op)
        {
            const int i = c.rng.nextInt (numSteps);
            auto& st = bar[i];
            if (st.locked) continue;
            const int t32 = (firstAbsStep + i) % 32;
            const bool anchor = c.isAnchor (t32);

            const float wSwap = 0.30f, wGhost = 0.20f, wRoll = 0.15f * (0.3f + c.s.rolls), wShift = 0.10f,
                        wRev = 0.15f * c.s.reverse, wPitch = 0.15f * c.s.pitchProb, wJump = 0.25f * c.s.chop;
            const float total = wSwap + wGhost + wRoll + wShift + wRev + wPitch + wJump;
            float r = c.rng.nextFloat() * total;

            if ((r -= wSwap) < 0)
            {
                if (st.on) { const int sl = c.pick (st.role); if (sl >= 0) st.slice = (int16_t) sl; }
            }
            else if ((r -= wGhost) < 0)
            {
                if (! st.on) { const int sl = c.pick (HitClass::Ghost); if (sl >= 0) st = c.makeStep (sl, HitClass::Ghost, 46); }
                else if (st.role == HitClass::Ghost) st = Step();
            }
            else if ((r -= wRoll) < 0)
            {
                if (st.on && ! anchor) st.roll = st.roll > 1 ? 1 : (uint8_t) (2 + c.rng.nextInt (3));
            }
            else if ((r -= wShift) < 0)
            {
                if (st.on && ! anchor)
                {
                    const int j = i + (c.chance (0.5f) ? 1 : -1);
                    if (j >= 0 && j < numSteps && ! bar[j].on && ! bar[j].locked && ! c.isAnchor ((firstAbsStep + j) % 32))
                        std::swap (bar[i], bar[j]);
                }
            }
            else if ((r -= wRev) < 0)
            {
                if (st.on && ! anchor) st.reverse = ! st.reverse;
            }
            else if ((r -= wPitch) < 0)
            {
                if (st.on) st.pitch = (int8_t) (st.pitch != 0 ? 0 : (c.chance (0.6f) ? 1 : -1) * (1 + c.rng.nextInt (juce::jmax (1, c.s.pitchRange))));
            }
            else
            {
                if (! anchor && ! c.goodStarts.empty())
                {
                    const int sl = c.sliceAtStep[(size_t) c.goodStart()];
                    if (sl >= 0) st = c.makeStep (sl, c.clsOf (sl), c.baseVelFor (c.clsOf (sl)));
                }
            }
        }
    }

    //==========================================================================
    void genFill (Ctx& c, Pattern& p)
    {
        auto& f = p.fill;
        for (int i = 0; i < kStepsPerBar; ++i) f[(size_t) i] = p.at (1, i);
        f.fill ({}); // start from the second motif bar's first half
        for (int i = 0; i < 8; ++i) f[(size_t) i] = p.at (1, i);

        const float amt = c.s.fillAmount;
        if (amt < 0.15f)
        {
            for (int i = 8; i < 16; ++i) f[(size_t) i] = p.at (1, i);
            mutateSteps (c, f.data(), 16, 16, 2);
            return;
        }

        const int start = amt < 0.4f ? 12 : 8;
        for (int i = start; i < 16; ++i) f[(size_t) i] = Step();

        const float r = c.rng.nextFloat();
        const float pStutter = 0.25f + 0.3f * c.s.stutter;
        const int type = r < 0.45f ? 0 : (r < 0.45f + pStutter ? 1 : 2);
        const bool ramp = c.s.pitchProb > 0.08f || c.chance (0.4f);

        if (type == 0) // snare roll
        {
            for (int i = start; i < 16; ++i)
            {
                if (i % 2 == 0 || c.chance (0.5f + 0.4f * amt))
                {
                    const int sl = c.pick (HitClass::Snare);
                    if (sl < 0) continue;
                    auto st = c.makeStep (sl, HitClass::Snare, 80 + (i - start) * 40 / juce::jmax (1, 16 - start));
                    if (i >= 14)      st.roll = (uint8_t) (c.chance (amt) ? 4 : 2);
                    else if (i >= 12) st.roll = (uint8_t) (c.chance (0.5f + 0.5f * amt) ? 2 : 1);
                    else              st.roll = (uint8_t) (c.chance (c.s.rolls) ? 2 : 1);
                    if (ramp) st.pitch = (int8_t) std::round ((float) (i - start) * 0.6f * amt);
                    f[(size_t) i] = st;
                }
            }
            if (start == 8) { const int k = c.pick (HitClass::Kick); if (k >= 0) f[8] = c.makeStep (k, HitClass::Kick, 118); }
        }
        else if (type == 1) // stutter on one slice, accelerating
        {
            const int sl = c.chance (0.7f) ? c.pick (HitClass::Snare) : c.sliceAtStep[(size_t) c.goodStart()];
            if (sl >= 0)
            {
                for (int i = start; i < 16; ++i)
                {
                    auto st = c.makeStep (sl, c.clsOf (sl), 92 + (i - start) * 4);
                    const int prog = (i - start) * 4 / juce::jmax (1, 16 - start);
                    st.roll = (uint8_t) juce::jlimit (1, 4, 1 + prog);
                    if (ramp) st.pitch = (int8_t) ((i - start) / 2);
                    st.gate = 0.6f + 0.4f * (float) (i - start) / (float) (16 - start);
                    f[(size_t) i] = st;
                }
            }
        }
        else // dropout then snare hits
        {
            const int sl = c.pick (HitClass::Snare);
            if (sl >= 0)
            {
                f[(size_t) start] = c.makeStep (sl, HitClass::Snare, 110);
                f[(size_t) start].gate = 0.3f;
                for (int i : { 12, 14, 15 })
                {
                    if (i <= start) continue;
                    auto st = c.makeStep (c.pick (HitClass::Snare), HitClass::Snare, 115);
                    if (i == 15) st.roll = (uint8_t) (c.chance (amt) ? 4 : 2);
                    if (ramp) st.pitch = (int8_t) (i - 12);
                    f[(size_t) i] = st;
                }
            }
        }

        if (c.chance (c.s.reverse * 0.6f) && f[15].on) f[15].reverse = true;
    }
}

//==============================================================================
Pattern Generator::generate (const Kit& kit, const GenSettings& s, uint32_t seed, const Pattern* previous)
{
    Pattern p;
    p.seed = seed;
    if (kit.slices.empty()) return p;

    Ctx c (kit, s, seed);

    // 2-bar motif
    genPhrase (c, p.steps.data(), 0, 32);

    // bars 3..8 : repetitions with variation, stronger at the end of each 4-bar phrase
    for (int b = 2; b < kMaxBars; ++b)
    {
        for (int i = 0; i < kStepsPerBar; ++i) p.at (b, i) = p.at (b % 2, i);
        const bool phraseEnd = (b % 4) == 3;
        if (phraseEnd && c.chance (s.variation * 0.8f))
        {
            std::array<Step, kStepsPerBar> tmp {};
            genPhrase (c, tmp.data(), b * kStepsPerBar, kStepsPerBar);
            for (int i = 8; i < 16; ++i) p.at (b, i) = tmp[(size_t) i];
        }
        const int ops = (int) std::round (s.variation * (phraseEnd ? 6.0f : 2.5f));
        mutateSteps (c, &p.at (b, 0), b * kStepsPerBar, kStepsPerBar, ops);
    }

    genFill (c, p);

    if (previous != nullptr)
    {
        for (size_t i = 0; i < p.steps.size(); ++i)
            if (previous->steps[i].locked) p.steps[i] = previous->steps[i];
        for (size_t i = 0; i < p.fill.size(); ++i)
            if (previous->fill[i].locked) p.fill[i] = previous->fill[i];
    }

    // safety : valid slice indices
    const int n = (int) kit.slices.size();
    for (auto& st : p.steps) if (st.slice >= n) st.slice = (int16_t) (n - 1);
    for (auto& st : p.fill)  if (st.slice >= n) st.slice = (int16_t) (n - 1);
    return p;
}

void Generator::mutate (Pattern& p, const Kit& kit, const GenSettings& s, uint32_t seed)
{
    if (kit.slices.empty()) return;
    Ctx c (kit, s, seed);
    const int opsPerBar = 1 + (int) std::round (s.variation * 3.0f + s.chop * 1.5f);
    for (int b = 0; b < kMaxBars; ++b)
        mutateSteps (c, &p.at (b, 0), b * kStepsPerBar, kStepsPerBar, opsPerBar);
    mutateSteps (c, p.fill.data(), kStepsPerBar, kStepsPerBar, opsPerBar);
    const int n = (int) kit.slices.size();
    for (auto& st : p.steps) if (st.slice >= n) st.slice = (int16_t) (n - 1);
    for (auto& st : p.fill)  if (st.slice >= n) st.slice = (int16_t) (n - 1);
}

Pattern Generator::remap (const Pattern& p, const Kit& oldKit, const Kit& newKit)
{
    Pattern out = p;
    if (newKit.slices.empty()) return out;
    const double ratio = (double) newKit.sourceSteps() / (double) oldKit.sourceSteps();

    auto mapStep = [&] (Step& st)
    {
        if (! st.on) return;
        double g = 0.0;
        if (st.slice >= 0 && st.slice < (int) oldKit.slices.size())
            g = oldKit.slices[(size_t) st.slice].gridPos * ratio;
        int best = -1; double bestD = 1.0e9;
        for (int pass = 0; pass < 2 && best < 0; ++pass)
        {
            for (int i = 0; i < (int) newKit.slices.size(); ++i)
            {
                if (pass == 0 && newKit.slices[(size_t) i].cls != st.role) continue;
                const double d = std::abs (newKit.slices[(size_t) i].gridPos - g);
                if (d < bestD) { bestD = d; best = i; }
            }
        }
        st.slice = (int16_t) juce::jmax (0, best);
    };
    for (auto& st : out.steps) mapStep (st);
    for (auto& st : out.fill)  mapStep (st);
    return out;
}

int Generator::naturalSliceForStep (const Kit& kit, int absoluteStep)
{
    if (kit.slices.empty()) return -1;
    const int src = absoluteStep % kit.sourceSteps();
    int best = 0; float bestD = 1.0e9f;
    for (int i = 0; i < (int) kit.slices.size(); ++i)
    {
        const float d = std::abs (kit.slices[(size_t) i].gridPos - (float) src);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

} // namespace bc
