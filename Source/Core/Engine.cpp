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
const Step* Engine::stepAt (int64_t k, int patternBars, int fillEvery) const
{
    if (k < 0) return nullptr;
    const int64_t bar = k / kStepsPerBar;
    const int s = (int) (k % kStepsPerBar);
    const bool fillBar = (fillEvery > 0 && ((bar + 1) % fillEvery) == 0) || bar == liveFillBar;
    if (fillBar) return &pattern.fill[(size_t) s];
    return &pattern.steps[(size_t) ((bar % patternBars) * kStepsPerBar + s)];
}

double Engine::distanceToNext (int64_t k, int j, int roll, const EngineParams& p) const
{
    if (j < roll - 1) return 1.0 / roll;
    const int len = p.patternBars * kStepsPerBar;
    for (int m = 1; m <= len; ++m)
    {
        auto* st = stepAt (k + m, p.patternBars, p.fillEvery);
        if (st != nullptr && st->on)
            return (double) m - (double) j / roll;
    }
    return (double) len;
}

void Engine::pushEvent (const Ev& e)
{
    if (numEvents >= kMaxEvents) return;
    // insertion keeps events sorted by offset (stable)
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

    for (int64_t k = kFirst; k <= kLast; ++k)
    {
        auto* st = stepAt (k, p.patternBars, p.fillEvery);
        if (st == nullptr || ! st->on) continue;

        const double off0 = (k % 2 == 1) ? swingOff : 0.0;
        const double off1 = ((k + 1) % 2 == 1) ? swingOff : 0.0;
        const double t0 = (double) k + off0, t1 = (double) (k + 1) + off1;
        const int roll = juce::jlimit (1, 4, (int) st->roll);

        const int64_t bar = k / kStepsPerBar;
        const bool fillBar = st >= pattern.fill.data() && st < pattern.fill.data() + kStepsPerBar;
        const int ui = fillBar ? kMaxSteps + (int) (k % kStepsPerBar)
                               : (int) ((bar % p.patternBars) * kStepsPerBar + k % kStepsPerBar);

        for (int j = 0; j < roll; ++j)
        {
            const double tStep = t0 + j * (t1 - t0) / roll;
            const double tPpq = tStep / 4.0;
            if (tPpq < ppqStart || tPpq >= ppqEnd) continue;

            Ev e;
            e.offset = startSample + juce::jlimit (0, numSamples - 1, (int) ((tPpq - ppqStart) * samplesPerPpq));
            e.slice = juce::jlimit (0, nSlices - 1, (int) st->slice);
            // later hits of a roll get a little quieter, like a drummer's buzz roll
            e.vel = (float) st->vel / 127.0f * (roll > 1 ? (1.0f - 0.08f * (float) j / roll) : 1.0f);
            e.reverse = st->reverse;
            e.pitch = (float) st->pitch;
            if (st->gate < 0.999f)
                e.gateSamples = (int64_t) juce::jmax (32.0, st->gate * distanceToNext (k, j, roll, p) * samplesPerStep);
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
    v->reverse = e.reverse;
    v->regionStart = juce::jlimit<int64_t> (0, len, sl.start);
    v->regionEnd = (tail == 0 && ! e.reverse) ? len : juce::jlimit<int64_t> (0, len, sl.end);

    const double srRatio = kit->audio->sampleRate / sr;
    const double pitchRatio = std::pow (2.0, (p.pitch + e.pitch) / 12.0);
    const double tempoRatio = juce::jlimit (0.25, 4.0, hostBpm / kit->bpm);
    const double dir = e.reverse ? -1.0 : 1.0;
    const double startPos = e.reverse ? (double) (v->regionEnd - 1) : (double) v->regionStart;

    v->cyclic = p.tempoMode == 2;
    if (v->cyclic)
    {
        v->anchor = startPos;
        v->anchorInc = dir * srRatio * tempoRatio;
        v->grainInc = dir * srRatio * pitchRatio;
        v->gA = v->gB = startPos;
        v->cycleLen = juce::jmax (16, (int) (p.cycleMs * 0.001f * (float) sr));
        v->xfadeLen = juce::jmax (1, juce::jmin (v->cycleLen / 4, (int) (0.001 * sr)));
        v->cycleCount = 0;
        v->xfadeCount = 0;
    }
    else
    {
        v->pos = startPos;
        v->inc = dir * srRatio * pitchRatio * (p.tempoMode == 0 ? tempoRatio : 1.0);
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
    const float releaseLen = (float) (0.003 * sr);

    for (auto& v : voices)
    {
        if (! v.active) continue;
        for (int i = start; i < end; ++i)
        {
            float l, r;
            double head, headInc;
            if (v.cyclic)
            {
                if (v.cycleCount >= v.cycleLen)
                {
                    v.gB = v.gA;
                    v.gA = v.anchor;
                    v.xfadeCount = v.xfadeLen;
                    v.cycleCount = 0;
                }
                l = hermite (srcL, len, v.gA);
                r = hermite (srcR, len, v.gA);
                if (v.xfadeCount > 0)
                {
                    const float a = (float) v.xfadeCount / (float) v.xfadeLen; // weight of the old grain
                    l = l * (1.0f - a) + hermite (srcL, len, v.gB) * a;
                    r = r * (1.0f - a) + hermite (srcR, len, v.gB) * a;
                    --v.xfadeCount;
                }
                v.gA += v.grainInc;
                v.gB += v.grainInc;
                head = v.anchor; headInc = v.anchorInc;
                v.anchor += v.anchorInc;
                ++v.cycleCount;
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

            if ((headInc >= 0.0 && head >= (double) v.regionEnd) || (headInc < 0.0 && head < (double) v.regionStart))
            {
                v.active = false;
                break;
            }
        }
    }
}

//==============================================================================
void Engine::applyFx (juce::AudioBuffer<float>& out, const EngineParams& p)
{
    const int n = out.getNumSamples();
    float* L = out.getWritePointer (0);
    float* R = out.getWritePointer (out.getNumChannels() > 1 ? 1 : 0);

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

    if (p.filterOn)
    {
        using T = juce::dsp::StateVariableTPTFilterType;
        filter.setType (p.filterType == 0 ? T::lowpass : (p.filterType == 1 ? T::bandpass : T::highpass));
        filter.setResonance (0.5f + juce::jlimit (0.0f, 1.0f, p.reso) * 7.5f);
        cutoffSmooth.setTargetValue (juce::jlimit (20.0f, (float) (sr * 0.45), p.cutoff));
        // update the cutoff in small chunks so automation stays smooth
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
        const bool fillBar = (p.fillEvery > 0 && ((bar + 1) % p.fillEvery) == 0) || bar == liveFillBar;
        uiStep.store (fillBar ? kMaxSteps + (int) (kNow % kStepsPerBar) : (int) ((bar % p.patternBars) * kStepsPerBar + kNow % kStepsPerBar));
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

    // play cursor feedback : most recent active voice
    const Voice* newest = nullptr;
    for (auto& v : voices) if (v.active && (newest == nullptr || v.age > newest->age)) newest = &v;
    uiSamplePos.store (newest != nullptr ? (newest->cyclic ? newest->anchor : newest->pos) : -1.0);

    applyFx (out, p);
}

} // namespace bc
