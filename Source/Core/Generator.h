#pragma once

#include "Types.h"

namespace bc
{

/** Effective generator settings (detail parameters after the macro knobs were applied). */
struct GenSettings
{
    int   style      = 0;
    float density    = 0.7f;   // amount of programmed hits from the style template
    float backbone   = 0.6f;   // how strictly the style's kick/snare skeleton is enforced
    float ghosts     = 0.5f;   // ghost-snare probability
    float hats       = 0.3f;   // hat / ride probability
    float chop       = 0.5f;   // probability that a cut block jumps elsewhere in the break (Collins' cut procedure)
    float cut16      = 0.2f;   // probability of cutting in 16ths instead of 8ths
    float rolls      = 0.3f;   // snare roll probability
    float stutter    = 0.2f;   // phrase-ending stutter probability
    float reverse    = 0.1f;   // reversed hits (reverse snare into snare)
    float pitchProb  = 0.1f;   // per-hit pitch change probability
    int   pitchRange = 3;      // semitones
    float gateProb   = 0.1f;   // shortened hits ("tiny gaps")
    float variation  = 0.4f;   // bar-to-bar variation
    float fillAmount = 0.6f;   // fill intensity
    int   maxRepeats = 2;      // repeats per cut block
    float humanize   = 0.3f;   // velocity humanisation
};

/** Style preset : rhythmic template (2 bars of 16ths, probabilities '.'=0, '1'..'9'=0.1..0.9, 'X'=1)
    plus default values for every detail parameter. */
struct StyleDef
{
    const char* name;
    const char* kick;
    const char* snare;
    const char* ghost;
    const char* hat;
    const char* anchors;    // e.g. "K0 S4 K10 S12" (positions inside the 32-step template)
    GenSettings defaults;
    int   tailMode;         // 0 = chunk (break keeps running), 1 = hit (stops at slice end)
    int   tempoMode;        // 0 = repitch, 1 = slice, 2 = cyclic stretch
    float swing;
};

const std::vector<StyleDef>& getStyles();
juce::StringArray getStyleNames();

class Generator
{
public:
    /** Builds a complete 8-bar pattern + fill bar. Locked steps of 'previous' are kept. */
    static Pattern generate (const Kit& kit, const GenSettings& s, uint32_t seed, const Pattern* previous);

    /** Small musical changes on unlocked steps. */
    static void mutate (Pattern& p, const Kit& kit, const GenSettings& s, uint32_t seed);

    /** Re-targets a pattern to another break, slice by slice, keeping the role (kick/snare/...) of each hit. */
    static Pattern remap (const Pattern& p, const Kit& oldKit, const Kit& newKit);

    /** The slice that naturally sits at a given step of the break (used when the user adds a step by hand). */
    static int naturalSliceForStep (const Kit& kit, int absoluteStep);
};

} // namespace bc
