#pragma once

#include "Engine.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace bc
{

struct Exporter
{
    /** Bars needed so that the export contains the pattern and, if enabled, its fill. */
    static int exportBars (const EngineParams& p)
    {
        int bars = p.patternBars;
        if (p.fillEvery > 0) bars = juce::jmax (bars, p.fillEvery);
        return juce::jlimit (1, 16, bars);
    }

    /** MIDI clip : one note per hit (C1 = slice 1). Pitch / reverse cannot be expressed in MIDI. */
    static bool writeMidi (const Pattern& pat, const EngineParams& p, int bars, double bpm, const juce::File& file)
    {
        constexpr int tpq = 960;
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / bpm)), 0.0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        const double swingOff = juce::jlimit (0.0f, 1.0f, p.swing) / 3.0;

        for (int k = 0; k < bars * kStepsPerBar; ++k)
        {
            const int bar = k / kStepsPerBar, s = k % kStepsPerBar;
            const bool fillBar = p.fillEvery > 0 && ((bar + 1) % p.fillEvery) == 0;
            const Step& st = fillBar ? pat.fill[(size_t) s] : pat.steps[(size_t) ((bar % p.patternBars) * kStepsPerBar + s)];
            if (! st.on) continue;
            const double t0 = k + ((k % 2 == 1) ? swingOff : 0.0);
            const double t1 = k + 1 + (((k + 1) % 2 == 1) ? swingOff : 0.0);
            const int roll = juce::jlimit (1, 4, (int) st.roll);
            for (int j = 0; j < roll; ++j)
            {
                const double on = (t0 + j * (t1 - t0) / roll) / 4.0 * tpq;
                const double len = (t1 - t0) / roll / 4.0 * tpq * juce::jlimit (0.1f, 1.0f, st.gate) * 0.95;
                const int note = juce::jlimit (0, 127, kFirstSliceNote + st.slice);
                seq.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) st.vel), on);
                seq.addEvent (juce::MidiMessage::noteOff (1, note), on + juce::jmax (10.0, len));
            }
        }
        seq.updateMatchedPairs();
        seq.addEvent (juce::MidiMessage::endOfTrack(), bars * 4.0 * tpq);

        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (tpq);
        mf.addTrack (seq);
        file.deleteFile();
        juce::FileOutputStream os (file);
        if (! os.openedOk()) return false;
        return mf.writeTo (os, 1);
    }

    /** Renders the loop exactly as the plugin plays it (pitch, reverse, rolls, FX) to a WAV file. */
    static bool renderAudio (Kit::Ptr kit, const Pattern& pat, const EngineParams& p, int bars, double bpm,
                             double sampleRate, const juce::File& file)
    {
        if (kit == nullptr) return false;
        const int64_t total = (int64_t) std::round (bars * 4.0 * 60.0 / bpm * sampleRate);
        juce::AudioBuffer<float> outBuf (2, (int) total);
        outBuf.clear();

        Engine eng;
        const int block = 256;
        eng.prepare (sampleRate, block);
        eng.setKit (kit);
        eng.setPattern (pat, false);
        juce::AudioBuffer<float> tmp (2, block);
        juce::MidiBuffer midi;
        EngineParams pp = p;
        pp.seqOn = true;

        // render the loop twice and keep the second pass, so tails from the end of the loop
        // ring into its beginning exactly like in a looping DAW clip
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int64_t pos = 0; pos < total; pos += block)
            {
                const int n = (int) juce::jmin<int64_t> (block, total - pos);
                tmp.setSize (2, n, false, false, true);
                Transport t;
                t.playing = true;
                t.bpm = bpm;
                t.ppq = (double) pos / sampleRate * bpm / 60.0;
                eng.process (tmp, midi, t, pp);
                if (pass == 1)
                    for (int c = 0; c < 2; ++c)
                        outBuf.copyFrom (c, (int) pos, tmp, c, 0, n);
            }
        }

        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (file.createOutputStream());
        if (os == nullptr) return false;
        auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sampleRate).withNumChannels (2).withBitsPerSample (24));
        if (w == nullptr) return false;
        return w->writeFromAudioSampleBuffer (outBuf, 0, outBuf.getNumSamples());
    }
};

} // namespace bc
