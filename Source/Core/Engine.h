#pragma once

#include "Types.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <limits>

namespace bc
{

enum TempoMode { TempoRepitch = 0, TempoSlice, TempoCyclic, TempoStretch };

struct EngineParams
{
    int   patternSteps = 64;    // loop length in 16th steps (1..128)
    int   fillEvery   = 0;      // 0 = off, else the last bar of every N bars plays the fill row
    float swing       = 0.0f;   // 0..1, 1 = full triplet shuffle
    int   tempoMode   = TempoRepitch;
    float cycleMs     = 40.0f;  // grain / cycle length for the stretch modes
    float pitch       = 0.0f;   // global semitones
    bool  poly        = false;
    int   tailMode    = 0;      // 0 = chunk (break keeps running until next hit), 1 = hit (stop at slice end)
    float decay       = 0.0f;   // 0 = natural, 1 = very tight
    float attackMs    = 0.5f;
    float velSens     = 0.7f;
    bool  seqOn       = true;

    // colour
    bool  crushOn  = false; float bits = 12.0f; float rate = 26000.0f;
    bool  filterOn = false; int filterType = 0; float cutoff = 20000.0f; float reso = 0.2f;
    bool  driveOn  = false; float drive = 0.3f;
    bool  compOn   = false; float compAmt = 0.5f; float compAtkMs = 3.0f; float compRelMs = 80.0f; float compMix = 0.5f;
    // space
    bool  dlyOn  = false; int dlyTime = 3; float dlyFb = 0.4f; float dlyTone = 0.6f; bool dlyPing = true; float dlyMix = 0.25f; int dlySrc = 2;   // src : 0 all, 1 snares + marked steps, 2 marked steps only
    bool  revOn  = false; float revSize = 0.45f; float revDamp = 0.5f; float revWidth = 1.0f; float revMix = 0.2f; int revSrc = 2;
    float outDb  = 0.0f;
};

/** Delay times offered by the delay (in 16th notes). */
inline const float* delayTimeSteps()
{
    static const float t[] = { 1.0f, 4.0f / 3.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };
    return t;
}
inline juce::StringArray delayTimeNames() { return { "1/16", "1/8 T", "1/8", "3/16", "1/4", "3/8", "1/2" }; }

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
    std::atomic<int>    uiStep { -1 };          // 0..127 normal steps, 128..143 fill bar
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
        uint8_t fx = 0;
    };

    struct Voice
    {
        bool    active = false;
        int     slice = -1;
        bool    send = false;                     // snare / ghost : feeds the "snares" sends
        uint8_t fx = 0;                           // per-step effects (StepFx)
        float   lfPhase = 0.0f, lfL = 0.0f, lfR = 0.0f;                    // per-hit lo-fi
        float   hpL = 0.0f, hpR = 0.0f, xpL = 0.0f, xpR = 0.0f, lpL = 0.0f, lpR = 0.0f;   // per-hit filter
        int64_t regionStart = 0, regionEnd = 0;
        bool    reverse = false;
        double  pos = 0.0, inc = 1.0;
        int     mode = TempoRepitch;
        double  anchor = 0.0, anchorInc = 1.0, grainInc = 1.0;
        double  g[2] = { 0.0, 0.0 };              // grain read heads
        int     gPhase[2] = { 0, 0 };
        int     cycleLen = 1000, cycleCount = 0, xfadeLen = 32, xfadeCount = 0;
        float   gain = 1.0f, env = 0.0f, attackInc = 1.0f, releaseDec = 0.01f, decayMul = 1.0f, decayEnv = 1.0f;
        int     stage = 0;                        // 0 attack, 1 sustain, 2 release
        int64_t gateLeft = std::numeric_limits<int64_t>::max();
        uint64_t age = 0;
    };

    static constexpr int kMaxVoices = 32;
    static constexpr int kMaxEvents = 1024;

    const Step* stepAt (int64_t k, const EngineParams& p) const;
    bool isFillBar (int64_t bar, int fillEvery) const;
    double distanceToNext (int64_t k, int j, int roll, const EngineParams& p) const;
    void collectSeqEvents (double ppqStart, int startSample, int numSamples, double samplesPerPpq, const EngineParams& p);
    void pushEvent (const Ev& e);
    void trigger (const Ev& e, const EngineParams& p, double hostBpm);
    void renderVoices (juce::AudioBuffer<float>& out, int start, int end);
    void releaseAll (float ms);
    void applyFx (juce::AudioBuffer<float>& out, const EngineParams& p, double bpm);

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
    juce::AudioBuffer<float> dlyBuf, revBuf, wetBuf;
    const EngineParams* cur = nullptr;   // parameters of the block being rendered
    float crushPhase = 0.0f, holdL = 0.0f, holdR = 0.0f;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> cutoffSmooth, outGainSmooth;
    float compEnv = 0.0f;
    std::vector<float> dlyL, dlyR;
    int dlyWrite = 0;
    float dlyLpL = 0.0f, dlyLpR = 0.0f;
    juce::SmoothedValue<float> dlySamples;
    juce::Reverb reverb;
};

} // namespace bc
