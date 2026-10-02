#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Core/Analyzer.h"
#include "Core/Generator.h"
#include "Core/Engine.h"
#include <functional>
#include <optional>

namespace PID
{
    // macros + style
    inline constexpr const char* complexity = "complexity";
    inline constexpr const char* energy     = "energy";
    inline constexpr const char* chaos      = "chaos";
    inline constexpr const char* style      = "style";
    // generator details
    inline constexpr const char* density    = "density";
    inline constexpr const char* backbone   = "backbone";
    inline constexpr const char* ghosts     = "ghosts";
    inline constexpr const char* hats       = "hats";
    inline constexpr const char* chop       = "chop";
    inline constexpr const char* cut16      = "cut16";
    inline constexpr const char* rolls      = "rolls";
    inline constexpr const char* stutter    = "stutter";
    inline constexpr const char* reverse    = "reverse";
    inline constexpr const char* pitchProb  = "pitchProb";
    inline constexpr const char* pitchRange = "pitchRange";
    inline constexpr const char* gateProb   = "gateProb";
    inline constexpr const char* variation  = "variation";
    inline constexpr const char* fillAmt    = "fillAmt";
    inline constexpr const char* maxRepeats = "maxRepeats";
    inline constexpr const char* humanize   = "humanize";
    // pattern
    inline constexpr const char* bars       = "bars";
    inline constexpr const char* fillEvery  = "fillEvery";
    inline constexpr const char* swing      = "swing";
    inline constexpr const char* quantize   = "quantize";
    inline constexpr const char* seqOn      = "seqOn";
    // playback
    inline constexpr const char* tempoMode  = "tempoMode";
    inline constexpr const char* cycleMs    = "cycleMs";
    inline constexpr const char* pitch      = "pitch";
    inline constexpr const char* voiceMode  = "voiceMode";
    inline constexpr const char* tailMode   = "tailMode";
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* decay      = "decay";
    inline constexpr const char* velSens    = "velSens";
    // fx
    inline constexpr const char* crushOn    = "crushOn";
    inline constexpr const char* bits       = "bits";
    inline constexpr const char* rate       = "rate";
    inline constexpr const char* filterOn   = "filterOn";
    inline constexpr const char* filterType = "filterType";
    inline constexpr const char* cutoff     = "cutoff";
    inline constexpr const char* reso       = "reso";
    inline constexpr const char* driveOn    = "driveOn";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* output     = "output";
    // remote triggers (map them to buttons / clip envelopes in Ableton)
    inline constexpr const char* genTrig    = "genTrig";
    inline constexpr const char* mutTrig    = "mutTrig";
    inline constexpr const char* fillTrig   = "fillTrig";
}

//==============================================================================
class JSliceProcessor : public juce::AudioProcessor,
                        public juce::ChangeBroadcaster,
                        private juce::Timer,
                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    JSliceProcessor();
    ~JSliceProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "J-Slice"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- everything below : message thread only
    juce::AudioProcessorValueTreeState apvts;
    bc::Engine engine;

    bc::Kit::Ptr getKit() const              { return kit; }
    const bc::Pattern& getPattern() const    { return pattern; }
    const bc::SlicerSettings& getSlicerSettings() const { return slicerSettings; }
    double getAnalysisConfidence() const     { return analysis.confidence; }
    bool canUndo() const                     { return ! undoStack.empty(); }
    bool canRedo() const                     { return ! redoStack.empty(); }

    bool loadFile (const juce::File& f, juce::String& error);
    juce::File getCurrentFile() const;
    juce::File getSiblingFile (int delta) const;
    static juce::String getSupportedWildcard() { return "*.wav;*.wave;*.aif;*.aiff;*.aifc;*.flac;*.mp3;*.ogg"; }

    void setSlicerSettings (const bc::SlicerSettings& s);
    void setBreakTempo (double bpm, int bars, bool reslice);
    void redetectTempo();
    void updateSlices (std::vector<bc::Slice> slices, bool reclassify);

    void setPattern (const bc::Pattern& p, bool undoable);
    void modifyStep (int index, const std::function<void (bc::Step&)>& fn, bool undoable = true);   // 0..127 bars, 128..143 fill
    void generate (std::optional<uint32_t> seed = {});
    void mutate();
    void undo();
    void redo();
    void clearLocks();
    void requestFill()                { engine.requestFill(); }
    void previewSlice (int i)         { engine.previewSlice (i); }
    void applyStyle (int index);

    bc::GenSettings currentGenSettings() const;
    bc::EngineParams currentEngineParams() const;
    double getLastBpm() const         { return lastBpm.load(); }

    juce::File exportMidi();
    juce::File exportAudio();

    // preview transport when the host is stopped
    std::atomic<bool> internalPlay { false };
    std::atomic<bool> hostPlaying { false };

    // ---- global settings shared by every instance (favourite folders etc.)
    juce::PropertiesFile& settings()  { return *globalSettings; }
    juce::StringArray getFavourites() const;
    void setFavourites (const juce::StringArray& f);
    bool keepPatternOnLoad() const    { return globalSettings->getBoolValue ("keepPattern", true); }
    void setKeepPatternOnLoad (bool b){ globalSettings->setValue ("keepPattern", b); globalSettings->saveIfNeeded(); }

private:
    void timerCallback() override;
    void parameterChanged (const juce::String& id, float value) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static bc::AudioData::Ptr readAudio (const juce::File& f, juce::String& error);
    void setAudio (bc::AudioData::Ptr audio, bool keepPattern);
    void rebuildSlices();
    void pushKit();
    float pv (const char* id) const   { return apvts.getRawParameterValue (id)->load(); }

    bc::Kit::Ptr kit;
    bc::AnalysisResult analysis;
    bc::SlicerSettings slicerSettings;
    bc::Pattern pattern;
    std::vector<bc::Pattern> undoStack, redoStack;
    juce::CriticalSection stateLock;

    std::atomic<double> lastBpm { 172.0 };
    double internalPpq = 0.0;
    std::atomic<int> genRequests { 0 }, mutRequests { 0 }, fillRequests { 0 };
    int lastMidiGen = 0, lastMidiMut = 0, lastMidiUndo = 0, lastMidiSeq = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    std::unique_ptr<juce::PropertiesFile> globalSettings;
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JSliceProcessor)
};
