#pragma once

#include "Types.h"

namespace bc
{

struct SlicerSettings
{
    enum Mode { Smart = 0, Transient, Grid };
    int   mode         = Smart;
    float sensitivity  = 0.5f;   // 0..1 : how many hits become slices
    int   gridDivision = 16;     // slices per bar in Grid mode (4, 8, 16, 32)
    float prerollMs    = 1.5f;   // cut slightly before the transient so the attack is kept intact
};

struct Onset
{
    int64_t pos = 0;          // refined sample position (pre-roll applied)
    float   strength = 0.0f;  // normalised onset-detection-function peak 0..1
    float   levelDb = -60.0f; // peak level just after the onset
};

struct AnalysisResult
{
    std::vector<Onset> onsets;        // all candidate onsets (low threshold)
    double  bpm = 120.0;              // estimated original tempo
    int     bars = 1;                 // estimated length in bars
    double  confidence = 0.0;         // 0..1
    int64_t gridOrigin = 0;           // first downbeat
};

/** Offline analysis of a drum break:
      1. multi-band "super-flux" onset detection (kick / snare-body / snare-noise / hats bands
         are normalised separately so quiet hats and loud kicks are equally visible)
      2. sample-accurate onset refinement + zero-crossing snap
      3. tempo / bar-count estimation (onset autocorrelation + loop-length matching)
      4. musically aware slice selection ("Smart" mode)
      5. hit classification (kick / snare / hat / ghost) */
class Analyzer
{
public:
    static AnalysisResult analyze (const juce::AudioBuffer<float>& audio, double sampleRate);

    /** Re-computes bars / grid origin for a user supplied tempo. */
    static int barsForTempo (int64_t numSamples, double sampleRate, double bpm, int64_t origin);

    static std::vector<Slice> makeSlices (const AnalysisResult& analysis,
                                          const juce::AudioBuffer<float>& audio, double sampleRate,
                                          double bpm, int bars, int64_t gridOrigin,
                                          const SlicerSettings& settings);

    static void classify (std::vector<Slice>& slices, const juce::AudioBuffer<float>& audio,
                          double sampleRate, double bpm, int64_t gridOrigin);

    /** Fills in end positions and grid positions after slices were moved / added. */
    static void finaliseSlices (std::vector<Slice>& slices, int64_t numSamples,
                                double sampleRate, double bpm, int64_t gridOrigin);
};

} // namespace bc
