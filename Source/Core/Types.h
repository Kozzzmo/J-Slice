#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include <cstdint>

namespace bc
{

//==============================================================================
// Drum-hit classes detected in a break
enum class HitClass : uint8_t { Kick = 0, Snare, Hat, Ghost };
constexpr int kNumClasses = 4;

inline const char* hitClassLetter (HitClass c)
{
    static const char* l[] = { "K", "S", "H", "G" };
    return l[(int) c];
}

inline const char* hitClassName (HitClass c)
{
    static const char* l[] = { "Kick", "Snare", "Hat / Cymbal", "Ghost" };
    return l[(int) c];
}

//==============================================================================
constexpr int kStepsPerBar   = 16;          // 16th-note grid
constexpr int kMaxBars       = 8;
constexpr int kMaxSteps      = kStepsPerBar * kMaxBars;
constexpr int kFirstSliceNote = 36;         // C1 = slice 1 (same as Ableton Simpler slice mode)
constexpr int kMaxSlices     = 64;

// MIDI notes used as remote controls (below the slice range)
constexpr int kNoteGenerate  = 24;          // C0
constexpr int kNoteMutate    = 25;          // C#0
constexpr int kNoteFill      = 26;          // D0
constexpr int kNoteUndo      = 27;          // D#0
constexpr int kNoteSeqToggle = 28;          // E0

//==============================================================================
struct Slice
{
    int64_t start = 0;          // first sample (inclusive)
    int64_t end   = 0;          // last sample (exclusive)
    float   strength = 0.0f;    // onset strength 0..1
    float   levelDb  = -60.0f;  // peak level of the attack
    float   gridPos  = 0.0f;    // position in 16th steps from the grid origin
    float   lowF = 0, lowMidF = 0, midF = 0, highF = 0; // spectral energy fractions
    HitClass cls = HitClass::Ghost;
    bool    userClass = false;  // class forced by the user
};

//==============================================================================
struct Step
{
    bool     on       = false;
    int16_t  slice    = 0;
    uint8_t  vel      = 100;
    bool     reverse  = false;
    int8_t   pitch    = 0;      // semitones
    uint8_t  roll     = 1;      // 1 = single hit, 2..4 = re-triggers inside the step
    float    gate     = 1.0f;   // 1 = until next event, < 1 shortens the hit
    bool     locked   = false;
    HitClass role     = HitClass::Kick;

    bool operator== (const Step&) const = default;
};

struct Pattern
{
    std::array<Step, kMaxSteps>    steps {};
    std::array<Step, kStepsPerBar> fill {};
    uint32_t seed = 0;

    Step&       at (int bar, int s)       { return steps[(size_t) (bar * kStepsPerBar + s)]; }
    const Step& at (int bar, int s) const { return steps[(size_t) (bar * kStepsPerBar + s)]; }

    bool operator== (const Pattern&) const = default;

    juce::String toString() const
    {
        juce::String out;
        out << (int) seed << "|";
        auto writeStep = [&out] (const Step& s)
        {
            out << (s.on ? 1 : 0) << "," << s.slice << "," << (int) s.vel << "," << (s.reverse ? 1 : 0) << ","
                << (int) s.pitch << "," << (int) s.roll << "," << juce::String ((double) s.gate, 9) << ","
                << (s.locked ? 1 : 0) << "," << (int) s.role << ";";
        };
        for (auto& s : steps) writeStep (s);
        out << "|";
        for (auto& s : fill) writeStep (s);
        return out;
    }

    static Pattern fromString (const juce::String& str)
    {
        Pattern p;
        auto parts = juce::StringArray::fromTokens (str, "|", "");
        if (parts.size() < 3) return p;
        p.seed = (uint32_t) parts[0].getLargeIntValue();

        auto readSteps = [] (const juce::String& s, Step* dest, int max)
        {
            auto items = juce::StringArray::fromTokens (s, ";", "");
            for (int i = 0; i < juce::jmin (max, items.size()); ++i)
            {
                auto f = juce::StringArray::fromTokens (items[i], ",", "");
                if (f.size() < 9) continue;
                Step st;
                st.on      = f[0].getIntValue() != 0;
                st.slice   = (int16_t) f[1].getIntValue();
                st.vel     = (uint8_t) juce::jlimit (1, 127, f[2].getIntValue());
                st.reverse = f[3].getIntValue() != 0;
                st.pitch   = (int8_t) juce::jlimit (-24, 24, f[4].getIntValue());
                st.roll    = (uint8_t) juce::jlimit (1, 4, f[5].getIntValue());
                st.gate    = juce::jlimit (0.05f, 1.0f, f[6].getFloatValue());
                st.locked  = f[7].getIntValue() != 0;
                st.role    = (HitClass) juce::jlimit (0, kNumClasses - 1, f[8].getIntValue());
                dest[i] = st;
            }
        };
        readSteps (parts[1], p.steps.data(), kMaxSteps);
        readSteps (parts[2], p.fill.data(), kStepsPerBar);
        return p;
    }
};

//==============================================================================
// Immutable audio data, shared between kits
struct AudioData : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<AudioData>;
    juce::AudioBuffer<float> buffer;   // always 2 channels
    double sampleRate = 44100.0;
    juce::File file;
    juce::String name;
};

// A kit = audio + tempo information + slices. Replaced as a whole when anything changes.
struct Kit : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<Kit>;

    AudioData::Ptr audio;
    double  bpm  = 120.0;     // original tempo of the break
    int     bars = 1;         // length of the break in bars
    int64_t gridOrigin = 0;   // sample position of the first downbeat
    std::vector<Slice> slices;

    int64_t length() const          { return audio != nullptr ? audio->buffer.getNumSamples() : 0; }
    double  samplesPerStep() const  { return audio != nullptr ? audio->sampleRate * 60.0 / bpm / 4.0 : 1.0; }
    int     sourceSteps() const     { return juce::jmax (1, bars * kStepsPerBar); }

    Kit::Ptr cloneWithSlices (std::vector<Slice> newSlices) const
    {
        Kit::Ptr k = new Kit();
        k->audio = audio; k->bpm = bpm; k->bars = bars; k->gridOrigin = gridOrigin;
        k->slices = std::move (newSlices);
        return k;
    }
};

} // namespace bc
