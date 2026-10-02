#pragma once

#include "Types.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <limits>

namespace bc
{

struct EngineParams
{
    int   patternBars = 2;
    int   fillEvery   = 0;      // 0 = off, else every N bars
    float swing       = 0.0f;   // 0..1, 1 = full triplet shuffle
    int   tempoMode   = 0;      // 0 = repitch (vintage), 1 = slice (original speed), 2 = cyclic stretch (Akai)
    float cycleMs     = 40.0f;
    float pitch       = 0.0f;   // global semitones
    bool  poly        = false;
    int   tailMode    = 0;      // 0 = chunk (break keeps running until next hit), 1 = hit (stop at slice end)
    float decay       = 0.0f;   // 0 = natural, 1 = very tight
    float attackMs    = 0.5f;
    float velSens     = 0.7f;
    bool  seqOn       = true;

    bool  crushOn  = false; float bits = 12.0f; float rate = 26000.0f;
    bool  filterOn = false; int filterType = 0; float cutoff = 20000.0f; float reso = 0.2f;
    bool  driveOn  = false; float drive = 0.3f;
    float outDb    = 0.0f;
};

struct Transport
{
    bool   playing = false;
    double ppq = 0.0;
    double bpm = 120.0;
};

/** Real-time sampler / sequencer. Owns no UI state ; safe to use offline for rendering. */
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlockSize);

    // ---- message thread
    void setKit (Kit::Ptr newKit);
    void setPattern (const Pattern& p, bool quantiseToBar);

    // ---- any thread
    void requestFill()                 { fillRequested.store (true); }
    void previewSlice (int index)      { previewRequest.store (index); }
    void stopAll()                     { stopRequested.store (true); }

    // ---- audio thread
    void process (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const Transport& t, const EngineParams& p);

    // ---- feedback for the UI / processor
    std::atomic<int>    uiStep { -1 };          // 0..127 normal bars, 128..143 fill bar
    std::atomic<int>    uiSlice { -1 };
    std::atomic<double> uiSamplePos { -1.0 };   // position inside the source of the last voice
    std::atomic<int>    midiGenerate { 0 }, midiMutate { 0 }, midiUndo { 0 }, midiSeqToggle { 0 };

private:
    struct Ev
    {
        int   offset = 0;
        int   slice = 0;
        float vel = 1.0f;
        bool  reverse = false;
        float pitch = 0.0f;
        int64_t gateSamples = std::numeric_limits<int64_t>::max();
        int   uiStep = -1;
    };

    struct Voice
    {
        bool    active = false;
        int     slice = -1;
        int64_t regionStart = 0, regionEnd = 0;
        bool    reverse = false;
        double  pos = 0.0, inc = 1.0;
        bool    cyclic = false;
        double  anchor = 0.0, anchorInc = 1.0, grainInc = 1.0, gA = 0.0, gB = 0.0;
        int     cycleLen = 1000, cycleCount = 0, xfadeLen = 32, xfadeCount = 0;
        float   gain = 1.0f, env = 0.0f, attackInc = 1.0f, releaseDec = 0.01f, decayMul = 1.0f, decayEnv = 1.0f;
        int     stage = 0;                        // 0 attack, 1 sustain, 2 release
        int64_t gateLeft = std::numeric_limits<int64_t>::max();
        uint64_t age = 0;
    };

    static constexpr int kMaxVoices = 24;
    static constexpr int kMaxEvents = 512;

    const Step* stepAt (int64_t k, int patternBars, int fillEvery) const;
    double distanceToNext (int64_t k, int j, int roll, const EngineParams& p) const;
    void collectSeqEvents (double ppqStart, int startSample, int numSamples, double samplesPerPpq, const EngineParams& p);
    void pushEvent (const Ev& e);
    void trigger (const Ev& e, const EngineParams& p, double hostBpm);
    void renderVoices (juce::AudioBuffer<float>& out, int start, int end);
    void releaseAll (float ms);
    void applyFx (juce::AudioBuffer<float>& out, const EngineParams& p);

    double sr = 44100.0;
    int maxBlock = 512;

    Kit::Ptr kit, pendingKit;
    bool kitPending = false;
    juce::SpinLock kitLock;

    Pattern pattern, pendingPattern;
    bool patternPending = false, patternQuantise = false;
    juce::SpinLock patternLock;

    std::atomic<bool> fillRequested { false }, stopRequested { false };
    std::atomic<int>  previewRequest { -1 };
    int64_t liveFillBar = -1000;
    bool wasPlaying = false;

    std::array<Voice, kMaxVoices> voices;
    uint64_t voiceCounter = 0;
    std::array<Ev, kMaxEvents> events;
    int numEvents = 0;

    // FX state
    float crushPhase = 0.0f, holdL = 0.0f, holdR = 0.0f;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> cutoffSmooth, outGainSmooth;
};

} // namespace bc
