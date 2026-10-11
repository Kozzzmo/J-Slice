#include "Engine.h"
#include <cmath>

namespace bc
{

Engine::Engine()
{
    cutoffSmooth.setCurrentAndTargetValue (20000.0f);
    outGainSmooth.setCurrentAndTargetValue (1.0f);
}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    filter.prepare (spec);
    filter.reset();
    cutoffSmooth.reset (sr, 0.02);
    outGainSmooth.reset (sr, 0.02);
    for (auto& v : voices) v.active = false;
    crushPhase = 0.0f;
    compEnv = 0.0f;

    sendBuf.setSize (2, maxBlock, false, true, true);
    wetBuf.setSize (2, maxBlock, false, true, true);

    const auto dlyLen = (size_t) (sr * 4.0) + 8;
    dlyL.assign (dlyLen, 0.0f);
    dlyR.assign (dlyLen, 0.0f);
    dlyWrite = 0;
    dlyLpL = dlyLpR = 0.0f;
    dlySamples.reset (sr, 0.08);
    dlySamples.setCurrentAndTargetValue ((float) (sr * 0.25));

    reverb.setSampleRate (sr);
    reverb.reset();
}

void Engine::setKit (Kit::Ptr newKit)
{
    const juce::SpinLock::ScopedLockType sl (kitLock);
    pendingKit = std::move (newKit);
    kitPending = true;
}

void Engine::setPattern (const Pattern& p, bool quantiseToBar)
{
    const juce::SpinLock::ScopedLockType sl (patternLock);
    pendingPattern = p;
    patternPending = true;
    patternQuantise = quantiseToBar;
}

//==============================================================================
bool Engine::isFillBar (int64_t bar, int fillEvery) const
{
    return (fillEvery > 0 && ((bar + 1) % fillEvery) == 0) || bar == liveFillBar;
}

const Step* Engine::stepAt (int64_t k, const EngineParams& p) const
{
    if (k < 0) return nullptr;
    const int64_t bar = k / kStepsPerBar;
    if (isFillBar (bar, p.fillEvery)) return &pattern.fill[(size_t) (k % kStepsPerBar)];
    const int len = juce::jlimit (1, kMaxSteps, p.patternSteps);
    return &pattern.steps[(size_t) (k % len)];
}

double Engine::distanceToNext (int64_t k, int j, int roll, const EngineParams& p) const
{
    if (j < roll - 1) return 1.0 / roll;
    const int len = juce::jlimit (1, kMaxSteps, p.patternSteps);
    for (int m = 1; m <= len + kStepsPerBar; ++m)
    {
        auto* st = stepAt (k + m, p);
        if (st != nullptr && st->on)
            return (double) m - (double) j / roll;
    }
    return (double) len;
}

void Engine::pushEvent (const Ev& e)
{
    if (numEvents >= kMaxEvents) return;
    int i = numEvents++;
    while (i > 0 && events[(size_t) i - 1].offset > e.offset)
    {
        events[(size_t) i] = events[(size_t) i - 1];
        --i;
    }
    events[(size_t) i] = e;
}

void Engine::collectSeqEvents (double ppqStart, int startSample, int numSamples, double samplesPerPpq, const EngineParams& p)
{
    if (kit == nullptr || kit->slices.empty() || numSamples <= 0) return;
    const double ppqEnd = ppqStart + numSamples / samplesPerPpq;
    const int64_t kFirst = (int64_t) std::floor (ppqStart * 4.0) - 1;
    const int64_t kLast  = (int64_t) std::floor (ppqEnd * 4.0);
    const double samplesPerStep = samplesPerPpq / 4.0;
    const double swingOff = juce::jlimit (0.0f, 1.0f, p.swing) / 3.0;
    const int nSlices = (int) kit->slices.size();
    const int len = juce::jlimit (1, kMaxSteps, p.patternSteps);

    for (int64_t k = kFirst; k <= kLast; ++k)
    {
        auto* st = stepAt (k, p);
        if (st == nullptr || ! st->on) continue;

        const double off0 = (k % 2 == 1) ? swingOff : 0.0;
        const double off1 = ((k + 1) % 2 == 1) ? swingOff : 0.0;
        const double t0 = (double) k + off0, t1 = (double) (k + 1) + off1;
        const int roll = juce::jlimit (1, kMaxRoll, (int) st->roll);

        const bool fillBar = st >= pattern.fill.data() && st < pattern.fill.data() + kStepsPerBar;
        const int ui = fillBar ? kMaxSteps + (int) (k % kStepsPerBar) : (int) (k % len);

        for (int j = 0; j < roll; ++j)
        {
            const double tStep = t0 + j * (t1 - t0) / roll;
            const double tPpq = tStep / 4.0;
            if (tPpq < ppqStart || tPpq >= ppqEnd) continue;

            Ev e;
            e.offset = startSample + juce::jlimit (0, numSamples - 1, (int) ((tPpq - ppqStart) * samplesPerPpq));
            e.slice = juce::jlimit (0, nSlices - 1, (int) st->slice);
            // later hits of a roll get a little quieter, like a drummer's buzz roll
            e.vel = (float) st->vel / 127.0f * (roll > 1 ? (1.0f - 0.25f * (float) j / roll) : 1.0f);
            e.reverse = st->reverse;
            e.pitch = (float) st->pitch;
            if (st->gate < 0.999f)
                e.gateSamples = (int64_t) juce::jmax (32.0, st->gate * distanceToNext (k, j, roll, p) * samplesPerStep);
            else if (roll > 1)
                e.gateSamples = (int64_t) juce::jmax (32.0, distanceToNext (k, j, roll, p) * samplesPerStep);
            e.uiStep = ui;
            pushEvent (e);
        }
    }
}

//==============================================================================
void Engine::releaseAll (float ms)
{
    const float dec = 1.0f / juce::jmax (1.0f, ms * 0.001f * (float) sr);
    for (auto& v : voices)
        if (v.active && v.stage != 2) { v.stage = 2; v.releaseDec = dec; }
}

void Engine::trigger (const Ev& e, const EngineParams& p, double hostBpm)
{
    if (kit == nullptr || kit->audio == nullptr || e.slice < 0 || e.slice >= (int) kit->slices.size()) return;
    const auto& sl = kit->slices[(size_t) e.slice];
    const int64_t len = kit->length();

    if (! p.poly) releaseAll (2.0f);

    Voice* v = nullptr;
    for (auto& cand : voices) if (! cand.active) { v = &cand; break; }
    if (v == nullptr)
    {
        v = &voices[0];
        for (auto& cand : voices) if (cand.age < v->age) v = &cand;
    }

    const int tail = p.poly ? 1 : p.tailMode;
    v->active = true;
    v->slice = e.slice;
    v->send = sl.cls == HitClass::Snare || sl.cls == HitClass::Ghost;
    v->reverse = e.reverse;
    v->regionStart = juce::jlimit<int64_t> (0, len, sl.start);
    v->regionEnd = (tail == 0 && ! e.reverse) ? len : juce::jlimit<int64_t> (0, len, sl.end);

    const double srRatio = kit->audio->sampleRate / sr;
    const double pitchRatio = std::pow (2.0, (p.pitch + e.pitch) / 12.0);
    const double tempoRatio = juce::jlimit (0.25, 4.0, hostBpm / kit->bpm);
    const double dir = e.reverse ? -1.0 : 1.0;
    const double startPos = e.reverse ? (double) (v->regionEnd - 1) : (double) v->regionStart;

    v->mode = p.tempoMode;
    if (v->mode == TempoCyclic || v->mode == TempoStretch)
    {
        v->anchor = startPos;
        v->anchorInc = dir * srRatio * tempoRatio;
        v->grainInc = dir * srRatio * pitchRatio;
        v->cycleLen = juce::jmax (32, (int) (p.cycleMs * 0.001f * (float) sr));
        v->cycleCount = 0;
        v->xfadeLen = juce::jmax (1, juce::jmin (v->cycleLen / 4, (int) (0.001 * sr)));
        v->xfadeCount = 0;
        v->g[0] = v->g[1] = startPos;
        // clean stretch : two Hann grains half a cycle apart ; head 1 starts at full weight on the attack
        v->gPhase[0] = 0;
        v->gPhase[1] = v->cycleLen / 2;
    }
    else
    {
        v->pos = startPos;
        v->inc = dir * srRatio * pitchRatio * (p.tempoMode == TempoRepitch ? tempoRatio : 1.0);
    }

    const float vel = juce::jlimit (0.0f, 1.0f, e.vel);
    v->gain = (1.0f - p.velSens) + p.velSens * vel * vel;
    v->env = 0.0f;
    v->stage = 0;
    v->attackInc = 1.0f / juce::jmax (1.0f, p.attackMs * 0.001f * (float) sr);
    v->decayEnv = 1.0f;
    if (p.decay > 0.001f)
    {
        const double tau = 0.03 * std::pow (2.0 / 0.03, 1.0 - (double) p.decay);
        v->decayMul = (float) std::exp (-1.0 / (tau * sr));
    }
    else v->decayMul = 1.0f;
    v->gateLeft = e.gateSamples;
    v->releaseDec = 1.0f / juce::jmax (1.0f, 0.003f * (float) sr);
    v->age = ++voiceCounter;

    uiSlice.store (e.slice);
}

//==============================================================================
namespace
{
    inline float hermite (const float* d, int64_t len, double p)
    {
        const int64_t i = (int64_t) std::floor (p);
        const float f = (float) (p - (double) i);
        auto s = [d, len] (int64_t k) { return (k >= 0 && k < len) ? d[k] : 0.0f; };
        const float xm1 = s (i - 1), x0 = s (i), x1 = s (i + 1), x2 = s (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    inline float hann (int phase, int len)
    {
        return 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) phase / (float) len);
    }
}

void Engine::renderVoices (juce::AudioBuffer<float>& out, int start, int end)
{
    if (end <= start || kit == nullptr || kit->audio == nullptr) return;
    const auto& buf = kit->audio->buffer;
    const int64_t len = buf.getNumSamples();
    const float* srcL = buf.getReadPointer (0);
    const float* srcR = buf.getReadPointer (buf.getNumChannels() > 1 ? 1 : 0);
    float* oL = out.getWritePointer (0);
    float* oR = out.getWritePointer (out.getNumChannels() > 1 ? 1 : 0);
    float* sL = sendBuf.getWritePointer (0);
    float* sR = sendBuf.getWritePointer (1);
    const float releaseLen = (float) (0.003 * sr);

    for (auto& v : voices)
    {
        if (! v.active) continue;
        for (int i = start; i < end; ++i)
        {
            float l = 0.0f, r = 0.0f;
            double head, headInc;
            if (v.mode == TempoCyclic)
            {
                // Akai-style : a single read head jumps back to the moving anchor every cycle
                if (v.cycleCount >= v.cycleLen)
                {
                    v.g[1] = v.g[0];
                    v.g[0] = v.anchor;
                    v.xfadeCount = v.xfadeLen;
                    v.cycleCount = 0;
                }
                l = hermite (srcL, len, v.g[0]);
                r = hermite (srcR, len, v.g[0]);
                if (v.xfadeCount > 0)
                {
                    const float a = (float) v.xfadeCount / (float) v.xfadeLen;
                    l = l * (1.0f - a) + hermite (srcL, len, v.g[1]) * a;
                    r = r * (1.0f - a) + hermite (srcR, len, v.g[1]) * a;
                    --v.xfadeCount;
                }
                v.g[0] += v.grainInc;
                v.g[1] += v.grainInc;
                head = v.anchor; headInc = v.anchorInc;
                v.anchor += v.anchorInc;
                ++v.cycleCount;
            }
            else if (v.mode == TempoStretch)
            {
                // clean overlap-add : two Hann-windowed grains, constant overall gain
                for (int h = 0; h < 2; ++h)
                {
                    const float w = hann (v.gPhase[h], v.cycleLen);
                    l += w * hermite (srcL, len, v.g[h]);
                    r += w * hermite (srcR, len, v.g[h]);
                    v.g[h] += v.grainInc;
                    if (++v.gPhase[h] >= v.cycleLen)
                    {
                        v.gPhase[h] = 0;
                        v.g[h] = v.anchor;
                    }
                }
                head = v.anchor; headInc = v.anchorInc;
                v.anchor += v.anchorInc;
            }
            else
            {
                l = hermite (srcL, len, v.pos);
                r = hermite (srcR, len, v.pos);
                head = v.pos; headInc = v.inc;
                v.pos += v.inc;
            }

            // envelope
            if (v.stage == 0)
            {
                v.env += v.attackInc;
                if (v.env >= 1.0f) { v.env = 1.0f; v.stage = 1; }
            }
            else if (v.stage == 2)
            {
                v.env -= v.releaseDec;
                if (v.env <= 0.0f) { v.active = false; break; }
            }
            if (v.stage != 2)
            {
                v.decayEnv *= v.decayMul;
                const double remaining = headInc >= 0.0 ? ((double) v.regionEnd - head) / juce::jmax (1.0e-6, headInc)
                                                         : (head - (double) v.regionStart) / juce::jmax (1.0e-6, -headInc);
                if (--v.gateLeft <= 0 || remaining <= releaseLen)
                {
                    v.stage = 2;
                    v.releaseDec = juce::jmax (v.env / juce::jmax (1.0f, (float) juce::jmin ((double) releaseLen, remaining)), 1.0e-4f);
                }
                if (v.decayEnv < 1.0e-4f) { v.active = false; break; }
            }

            const float g = v.gain * v.env * v.decayEnv;
            oL[i] += l * g;
            oR[i] += r * g;
            if (v.send) { sL[i] += l * g; sR[i] += r * g; }

            if ((headInc >= 0.0 && head >= (double) v.regionEnd) || (headInc < 0.0 && head < (double) v.regionStart))
            {
                v.active = false;
                break;
            }
        }
    }
}

//==============================================================================
void Engine::applyFx (juce::AudioBuffer<float>& out, const EngineParams& p, double bpm)
{
    const int n = out.getNumSamples();
    float* L = out.getWritePointer (0);
    float* R = out.getWritePointer (out.getNumChannels() > 1 ? 1 : 0);

    // ---- vintage sampler : sample-rate + bit reduction
    if (p.crushOn)
    {
        const float step = juce::jlimit (0.0f, 1.0f, p.rate / (float) sr);
        const float q = std::pow (2.0f, juce::jlimit (2.0f, 24.0f, p.bits) - 1.0f);
        for (int i = 0; i < n; ++i)
        {
            crushPhase += step;
            if (crushPhase >= 1.0f || step >= 1.0f)
            {
                crushPhase -= std::floor (crushPhase);
                holdL = L[i]; holdR = R[i];
            }
            L[i] = std::round (holdL * q) / q;
            R[i] = std::round (holdR * q) / q;
        }
    }

    // ---- filter
    if (p.filterOn)
    {
        using T = juce::dsp::StateVariableTPTFilterType;
        filter.setType (p.filterType == 0 ? T::lowpass : (p.filterType == 1 ? T::bandpass : T::highpass));
        filter.setResonance (0.5f + juce::jlimit (0.0f, 1.0f, p.reso) * 7.5f);
        cutoffSmooth.setTargetValue (juce::jlimit (20.0f, (float) (sr * 0.45), p.cutoff));
        for (int s = 0; s < n; s += 32)
        {
            const int c = juce::jmin (32, n - s);
            filter.setCutoffFrequency (cutoffSmooth.getNextValue());
            cutoffSmooth.skip (c - 1);
            for (int i = s; i < s + c; ++i)
            {
                L[i] = filter.processSample (0, L[i]);
                R[i] = filter.processSample (1, R[i]);
            }
        }
    }
    else
    {
        cutoffSmooth.setCurrentAndTargetValue (juce::jlimit (20.0f, (float) (sr * 0.45), p.cutoff));
    }

    // ---- drive
    if (p.driveOn)
    {
        const float d = 1.0f + 15.0f * p.drive * p.drive;
        const float norm = 1.0f / std::tanh (d);
        for (int i = 0; i < n; ++i)
        {
            L[i] = std::tanh (L[i] * d) * norm;
            R[i] = std::tanh (R[i] * d) * norm;
        }
    }

    // ---- parallel compressor ("squash" : the classic way to make a break slam)
    if (p.compOn)
    {
        const float thrDb = -36.0f * juce::jlimit (0.0f, 1.0f, p.compAmt);
        const float ratio = 4.0f + 16.0f * p.compAmt;
        const float makeup = juce::Decibels::decibelsToGain (-thrDb * (1.0f - 1.0f / ratio) * 0.6f);
        const float atk = std::exp (-1.0f / (juce::jmax (0.1f, p.compAtkMs) * 0.001f * (float) sr));
        const float rel = std::exp (-1.0f / (juce::jmax (5.0f, p.compRelMs) * 0.001f * (float) sr));
        const float mix = juce::jlimit (0.0f, 1.0f, p.compMix);
        for (int i = 0; i < n; ++i)
        {
            const float lvl = juce::jmax (std::abs (L[i]), std::abs (R[i]));
            compEnv = lvl > compEnv ? atk * compEnv + (1.0f - atk) * lvl : rel * compEnv + (1.0f - rel) * lvl;
            const float envDb = juce::Decibels::gainToDecibels (compEnv, -100.0f);
            const float over = envDb - thrDb;
            const float grDb = over > 0.0f ? -over * (1.0f - 1.0f / ratio) : 0.0f;
            const float g = juce::Decibels::decibelsToGain (grDb) * makeup;
            L[i] = L[i] * (1.0f - mix) + std::tanh (L[i] * g) * mix;
            R[i] = R[i] * (1.0f - mix) + std::tanh (R[i] * g) * mix;
        }
    }

    // ---- sends : delay and reverb take either the whole mix or only the snares / ghosts
    wetBuf.clear (0, 0, n);
    wetBuf.clear (1, 0, n);
    float* wL = wetBuf.getWritePointer (0);
    float* wR = wetBuf.getWritePointer (1);
    const float* sendL = sendBuf.getReadPointer (0);
    const float* sendR = sendBuf.getReadPointer (1);

    if (p.dlyOn)
    {
        const int idx = juce::jlimit (0, 6, p.dlyTime);
        const double samples = delayTimeSteps()[idx] * 15.0 / juce::jmax (30.0, bpm) * sr;
        dlySamples.setTargetValue ((float) juce::jlimit (1.0, (double) dlyL.size() - 4.0, samples));
        const int size = (int) dlyL.size();
        const float fb = juce::jlimit (0.0f, 0.95f, p.dlyFb);
        const float lpCoef = 0.05f + 0.9f * juce::jlimit (0.0f, 1.0f, p.dlyTone);
        const float mix = p.dlyMix;
        for (int i = 0; i < n; ++i)
        {
            const float d = dlySamples.getNextValue();
            float rp = (float) dlyWrite - d;
            while (rp < 0.0f) rp += (float) size;
            const int i0 = (int) rp;
            const float fr = rp - (float) i0;
            const int i1 = (i0 + 1) % size;
            const float yl = dlyL[(size_t) i0] * (1.0f - fr) + dlyL[(size_t) i1] * fr;
            const float yr = dlyR[(size_t) i0] * (1.0f - fr) + dlyR[(size_t) i1] * fr;
            dlyLpL += lpCoef * (yl - dlyLpL);
            dlyLpR += lpCoef * (yr - dlyLpR);

            const float inL = p.dlySrc == 0 ? L[i] : sendL[i];
            const float inR = p.dlySrc == 0 ? R[i] : sendR[i];
            if (p.dlyPing)
            {
                dlyL[(size_t) dlyWrite] = 0.5f * (inL + inR) + fb * dlyLpR;
                dlyR[(size_t) dlyWrite] = fb * dlyLpL;
            }
            else
            {
                dlyL[(size_t) dlyWrite] = inL + fb * dlyLpL;
                dlyR[(size_t) dlyWrite] = inR + fb * dlyLpR;
            }
            dlyWrite = (dlyWrite + 1) % size;
            wL[i] += dlyLpL * mix;
            wR[i] += dlyLpR * mix;
        }
    }
    else
    {
        dlyLpL = dlyLpR = 0.0f;
    }

    if (p.revOn)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = juce::jlimit (0.0f, 1.0f, p.revSize);
        rp.damping = juce::jlimit (0.0f, 1.0f, p.revDamp);
        rp.width = juce::jlimit (0.0f, 1.0f, p.revWidth);
        rp.wetLevel = 1.0f;
        rp.dryLevel = 0.0f;
        reverb.setParameters (rp);
        // reuse the send buffer as the reverb input (it is not needed any more after the delay)
        float* inL = sendBuf.getWritePointer (0);
        float* inR = sendBuf.getWritePointer (1);
        if (p.revSrc == 0)
        {
            juce::FloatVectorOperations::copy (inL, L, n);
            juce::FloatVectorOperations::copy (inR, R, n);
        }
        reverb.processStereo (inL, inR, n);
        juce::FloatVectorOperations::addWithMultiply (wL, inL, p.revMix, n);
        juce::FloatVectorOperations::addWithMultiply (wR, inR, p.revMix, n);
    }

    juce::FloatVectorOperations::add (L, wL, n);
    juce::FloatVectorOperations::add (R, wR, n);

    outGainSmooth.setTargetValue (juce::Decibels::decibelsToGain (p.outDb));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainSmooth.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }
}

//==============================================================================
void Engine::process (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const Transport& t, const EngineParams& p)
{
    const int n = out.getNumSamples();
    out.clear();
    numEvents = 0;
    if (sendBuf.getNumSamples() < n)
    {
        // host sent a bigger block than announced : grow (rare, not real-time safe but better than crashing)
        sendBuf.setSize (2, n, false, true, true);
        wetBuf.setSize (2, n, false, true, true);
    }
    sendBuf.clear (0, 0, n);
    sendBuf.clear (1, 0, n);

    {
        const juce::SpinLock::ScopedTryLockType tl (kitLock);
        if (tl.isLocked() && kitPending)
        {
            std::swap (kit, pendingKit);  // old kit is released later on the message thread
            kitPending = false;
            for (auto& v : voices) v.active = false;
        }
    }

    bool patternWaiting = false;
    {
        const juce::SpinLock::ScopedTryLockType tl (patternLock);
        if (tl.isLocked() && patternPending)
        {
            if (! patternQuantise || ! t.playing) { pattern = pendingPattern; patternPending = false; }
            else patternWaiting = true;
        }
    }

    if (stopRequested.exchange (false)) releaseAll (5.0f);
    if (wasPlaying && ! t.playing) releaseAll (5.0f);
    wasPlaying = t.playing;

    const double bpm = t.bpm > 1.0 ? t.bpm : 120.0;
    const double samplesPerPpq = sr * 60.0 / bpm;

    if (fillRequested.exchange (false) && t.playing)
        liveFillBar = (int64_t) std::floor (t.ppq / 4.0) + 1;

    // ---- MIDI input
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (! m.isNoteOn()) continue;
        const int note = m.getNoteNumber();
        if (note == kNoteGenerate)       midiGenerate.fetch_add (1);
        else if (note == kNoteMutate)    midiMutate.fetch_add (1);
        else if (note == kNoteFill)      { if (t.playing) liveFillBar = (int64_t) std::floor (t.ppq / 4.0) + 1; }
        else if (note == kNoteUndo)      midiUndo.fetch_add (1);
        else if (note == kNoteSeqToggle) midiSeqToggle.fetch_add (1);
        else if (note >= kFirstSliceNote && kit != nullptr && note - kFirstSliceNote < (int) kit->slices.size())
        {
            Ev e;
            e.offset = juce::jlimit (0, n - 1, meta.samplePosition);
            e.slice = note - kFirstSliceNote;
            e.vel = m.getFloatVelocity();
            pushEvent (e);
        }
    }

    const int pv = previewRequest.exchange (-1);
    if (pv >= 0) { Ev e; e.offset = 0; e.slice = pv; e.vel = 0.9f; pushEvent (e); }

    // ---- sequencer
    if (t.playing && p.seqOn && t.ppq >= 0.0)
    {
        int split = n;
        if (patternWaiting)
        {
            const double nextBar = (std::floor (t.ppq / 4.0) + 1.0) * 4.0;
            const double sToBar = (nextBar - t.ppq) * samplesPerPpq;
            if (sToBar < (double) n) split = juce::jmax (0, (int) std::ceil (sToBar));
        }
        collectSeqEvents (t.ppq, 0, split, samplesPerPpq, p);
        if (split < n)
        {
            const juce::SpinLock::ScopedTryLockType tl (patternLock);
            if (tl.isLocked() && patternPending) { pattern = pendingPattern; patternPending = false; }
            collectSeqEvents (t.ppq + split / samplesPerPpq, split, n - split, samplesPerPpq, p);
        }

        const int64_t kNow = (int64_t) std::floor ((t.ppq + n / samplesPerPpq) * 4.0);
        const int64_t bar = kNow / kStepsPerBar;
        const int len = juce::jlimit (1, kMaxSteps, p.patternSteps);
        uiStep.store (isFillBar (bar, p.fillEvery) ? kMaxSteps + (int) (kNow % kStepsPerBar) : (int) (kNow % len));
    }
    else if (! t.playing)
    {
        uiStep.store (-1);
    }

    // ---- render
    int cursor = 0;
    for (int i = 0; i < numEvents; ++i)
    {
        renderVoices (out, cursor, events[(size_t) i].offset);
        cursor = events[(size_t) i].offset;
        trigger (events[(size_t) i], p, bpm);
    }
    renderVoices (out, cursor, n);

    const Voice* newest = nullptr;
    for (auto& v : voices) if (v.active && (newest == nullptr || v.age > newest->age)) newest = &v;
    uiSamplePos.store (newest != nullptr ? (newest->mode >= TempoCyclic ? newest->anchor : newest->pos) : -1.0);

    applyFx (out, p, bpm);
}

} // namespace bc
