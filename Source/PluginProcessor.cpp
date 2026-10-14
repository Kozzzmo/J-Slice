#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/Exporter.h"

using namespace bc;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout JSliceProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; }));
    };
    const auto& d = getStyles()[0].defaults;

    // macros : -100 (left) = related settings at zero, 0 (centre) = the style's own settings, +100 = pushed to the maximum
    auto macro = [] (const char* id, const char* name)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                                      { const int n = roundToInt ((v - 0.5f) * 200.0f); return (n > 0 ? "+" : "") + String (n); }));
    };
    l.add (macro (PID::complexity, "Complexity"));
    l.add (macro (PID::energy, "Energy"));
    l.add (macro (PID::chaos, "Chaos"));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::style, 1 }, "Style", getStyleNames(), 0));

    l.add (pct (PID::density, "Density", d.density));
    l.add (pct (PID::backbone, "Backbone", d.backbone));
    l.add (pct (PID::ghosts, "Ghost notes", d.ghosts));
    l.add (pct (PID::hats, "Hats", d.hats));
    l.add (pct (PID::chop, "Chop", d.chop));
    l.add (pct (PID::cut16, "16th cuts", d.cut16));
    l.add (pct (PID::rolls, "Rolls", d.rolls));
    l.add (pct (PID::stutter, "Stutter", d.stutter));
    l.add (pct (PID::reverse, "Reverse", d.reverse));
    l.add (pct (PID::pitchProb, "Pitch prob", d.pitchProb));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { PID::pitchRange, 1 }, "Pitch range", 0, 12, d.pitchRange));
    l.add (pct (PID::gateProb, "Short hits", d.gateProb));
    l.add (pct (PID::variation, "Variation", d.variation));
    l.add (pct (PID::fillAmt, "Fill amount", d.fillAmount));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { PID::maxRepeats, 1 }, "Max repeats", 1, 4, d.maxRepeats));
    l.add (pct (PID::humanize, "Humanize", d.humanize));

    l.add (std::make_unique<AudioParameterInt> (ParameterID { PID::steps, 1 }, "Pattern length (steps)", 1, kMaxSteps, 64));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::fillOn, 1 }, "Fill on", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::fillEvery, 1 }, "Fill every", StringArray { "Every 2 bars", "Every 4 bars", "Every 8 bars" }, 1));
    l.add (pct (PID::swing, "Swing", 0.0f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::quantize, 1 }, "Change on next bar", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::seqOn, 1 }, "Sequencer on", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::syncOn, 1 }, "Sync to host", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::bOn, 1 }, "Break B on", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::duoMode, 1 }, "Duo mode", duoModeNames(), 0));
    l.add (pct (PID::bShare, "B share", 0.5f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::freeBpm, 1 }, "Free tempo", NormalisableRange<float> (60.0f, 220.0f, 0.1f), 172.0f,
                                                  AudioParameterFloatAttributes().withLabel ("BPM")));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::tempoMode, 1 }, "Tempo mode", StringArray { "Repitch (vintage)", "Slice (original speed)", "Cyclic stretch (Akai)", "Stretch (clean)" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::cycleMs, 1 }, "Cycle length", NormalisableRange<float> (5.0f, 120.0f, 0.1f, 0.6f), 40.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::pitch, 1 }, "Pitch", NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("st")));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::voiceMode, 1 }, "Voice mode", StringArray { "Mono", "Poly" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::tailMode, 1 }, "Slice tail", StringArray { "Chunk (break runs on)", "Hit (stop at slice end)" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::attack, 1 }, "Attack", NormalisableRange<float> (0.0f, 20.0f, 0.01f, 0.5f), 0.5f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (pct (PID::decay, "Tightness", 0.0f));
    l.add (pct (PID::velSens, "Velocity", 0.7f));

    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::crushOn, 1 }, "Vintage on", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::bits, 1 }, "Bits", NormalisableRange<float> (4.0f, 16.0f, 0.1f), 12.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::rate, 1 }, "Sample rate", NormalisableRange<float> (2000.0f, 48000.0f, 1.0f, 0.5f), 26040.0f,
                                                  AudioParameterFloatAttributes().withLabel ("Hz")));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::filterOn, 1 }, "Filter on", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::filterType, 1 }, "Filter type", StringArray { "Low-pass", "Band-pass", "High-pass" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::cutoff, 1 }, "Cutoff", NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.25f), 20000.0f,
                                                  AudioParameterFloatAttributes().withLabel ("Hz")));
    l.add (pct (PID::reso, "Resonance", 0.2f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::driveOn, 1 }, "Drive on", false));
    l.add (pct (PID::drive, "Drive", 0.3f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::compOn, 1 }, "Comp on", false));
    l.add (pct (PID::compAmt, "Squash", 0.5f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::compAtk, 1 }, "Comp attack", NormalisableRange<float> (0.1f, 50.0f, 0.01f, 0.4f), 3.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::compRel, 1 }, "Comp release", NormalisableRange<float> (10.0f, 600.0f, 0.1f, 0.5f), 80.0f,
                                                  AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (pct (PID::compMix, "Comp mix", 0.5f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::dlyOn, 1 }, "Delay on", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::dlyTime, 1 }, "Delay time", delayTimeNames(), 3));
    l.add (pct (PID::dlyFb, "Delay feedback", 0.4f));
    l.add (pct (PID::dlyTone, "Delay tone", 0.55f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::dlyPing, 1 }, "Ping-pong", true));
    l.add (pct (PID::dlyMix, "Delay level", 0.3f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::dlySrc, 1 }, "Delay source", StringArray { "All", "Snares+marked", "Marked only" }, 2));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::revOn, 1 }, "Reverb on", false));
    l.add (pct (PID::revSize, "Reverb size", 0.45f));
    l.add (pct (PID::revDamp, "Reverb damping", 0.5f));
    l.add (pct (PID::revWidth, "Reverb width", 1.0f));
    l.add (pct (PID::revMix, "Reverb level", 0.25f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::revSrc, 1 }, "Reverb source", StringArray { "All", "Snares+marked", "Marked only" }, 2));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { PID::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dB")));

    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::genTrig, 1 }, "Trigger: generate", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::mutTrig, 1 }, "Trigger: mutate", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::fillTrig, 1 }, "Trigger: fill", false));
    return l;
}


namespace
{
    // every parameter that changes the generated pattern : moving one re-generates live (same seed)
    const char* const kGenParams[] = { PID::complexity, PID::energy, PID::chaos, PID::density, PID::backbone, PID::ghosts,
                                       PID::hats, PID::chop, PID::cut16, PID::rolls, PID::stutter, PID::reverse,
                                       PID::pitchProb, PID::pitchRange, PID::gateProb, PID::variation, PID::fillAmt,
                                       PID::maxRepeats, PID::humanize, PID::duoMode, PID::bShare, PID::bOn };
}

//==============================================================================
JSliceProcessor::JSliceProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    formatManager.registerBasicFormats();

    juce::PropertiesFile::Options o;
    o.applicationName = "J-Slice";      // settings file kept from J-Slice (favourites, update check)
    o.folderName = "J-Slice";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    globalSettings = std::make_unique<juce::PropertiesFile> (o);

    for (auto* id : { PID::genTrig, PID::mutTrig, PID::fillTrig })
        apvts.addParameterListener (id, this);
    for (auto* id : kGenParams)
        apvts.addParameterListener (id, this);

    pattern.seed = 1;
    startTimerHz (30);
}

JSliceProcessor::~JSliceProcessor()
{
    *alive = false;
    stopTimer();
    for (auto* id : { PID::genTrig, PID::mutTrig, PID::fillTrig })
        apvts.removeParameterListener (id, this);
    for (auto* id : kGenParams)
        apvts.removeParameterListener (id, this);
}

bool JSliceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void JSliceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    for (int k = 0; k < kNumBreaks; ++k)
        if (breaks[(size_t) k].kit != nullptr) engine.setKit (k, breaks[(size_t) k].kit);
    engine.setPattern (pattern, false);
}

//==============================================================================
EngineParams JSliceProcessor::currentEngineParams() const
{
    EngineParams p;
    p.patternSteps = juce::jlimit (1, kMaxSteps, (int) pv (PID::steps));
    const int fe = (int) pv (PID::fillEvery);
    p.fillEvery = pv (PID::fillOn) > 0.5f ? (fe == 0 ? 2 : fe == 1 ? 4 : 8) : 0;
    p.swing = pv (PID::swing);
    p.tempoMode = (int) pv (PID::tempoMode);
    p.cycleMs = pv (PID::cycleMs);
    p.pitch = pv (PID::pitch);
    p.poly = (int) pv (PID::voiceMode) == 1;
    p.tailMode = (int) pv (PID::tailMode);
    p.attackMs = pv (PID::attack);
    p.decay = pv (PID::decay);
    p.velSens = pv (PID::velSens);
    p.seqOn = pv (PID::seqOn) > 0.5f;
    p.bOn = pv (PID::bOn) > 0.5f;
    p.crushOn = pv (PID::crushOn) > 0.5f;
    p.bits = pv (PID::bits);
    p.rate = pv (PID::rate);
    p.filterOn = pv (PID::filterOn) > 0.5f;
    p.filterType = (int) pv (PID::filterType);
    p.cutoff = pv (PID::cutoff);
    p.reso = pv (PID::reso);
    p.driveOn = pv (PID::driveOn) > 0.5f;
    p.drive = pv (PID::drive);
    p.compOn = pv (PID::compOn) > 0.5f;
    p.compAmt = pv (PID::compAmt);
    p.compAtkMs = pv (PID::compAtk);
    p.compRelMs = pv (PID::compRel);
    p.compMix = pv (PID::compMix);
    p.dlyOn = pv (PID::dlyOn) > 0.5f;
    p.dlyTime = (int) pv (PID::dlyTime);
    p.dlyFb = pv (PID::dlyFb);
    p.dlyTone = pv (PID::dlyTone);
    p.dlyPing = pv (PID::dlyPing) > 0.5f;
    p.dlyMix = pv (PID::dlyMix);
    p.dlySrc = (int) pv (PID::dlySrc);
    p.revOn = pv (PID::revOn) > 0.5f;
    p.revSize = pv (PID::revSize);
    p.revDamp = pv (PID::revDamp);
    p.revWidth = pv (PID::revWidth);
    p.revMix = pv (PID::revMix);
    p.revSrc = (int) pv (PID::revSrc);
    p.outDb = pv (PID::output);
    return p;
}

GenSettings JSliceProcessor::currentGenSettings() const
{
    // macros : 0 = every related setting at zero, 0.5 = the style's own detail settings,
    // 1 = every related setting pushed to the maximum
    const float c = pv (PID::complexity), e = pv (PID::energy), x = pv (PID::chaos);
    auto cl = [] (float v) { return juce::jlimit (0.0f, 1.0f, v); };
    auto push = [&cl] (float v, float m, float w)
    {
        if (m < 0.5f) return cl (v * m * 2.0f);                     // left half : fade to zero
        return cl (v + (1.0f - v) * (m - 0.5f) * 2.0f * w);        // right half : push up
    };

    GenSettings s;
    s.style      = (int) pv (PID::style);
    s.density    = push (pv (PID::density), e, 0.8f);
    s.ghosts     = push (pv (PID::ghosts), e, 1.0f);
    s.hats       = push (pv (PID::hats), e, 0.8f);
    s.chop       = push (pv (PID::chop), c, 1.0f);
    s.cut16      = push (pv (PID::cut16), c, 0.7f);
    s.rolls      = push (pv (PID::rolls), c, 0.8f);
    const int rep = (int) pv (PID::maxRepeats);
    s.maxRepeats = c < 0.5f ? juce::jmax (1, (int) std::round ((float) rep * c * 2.0f))
                            : juce::jlimit (1, 4, rep + (int) std::round ((c - 0.5f) * 2.0f * (float) (4 - rep)));
    s.fillAmount = push (pv (PID::fillAmt), c, 1.0f);
    s.stutter    = push (pv (PID::stutter), x, 0.8f);
    s.reverse    = push (pv (PID::reverse), x, 0.5f);
    s.pitchProb  = push (pv (PID::pitchProb), x, 0.5f);
    s.pitchRange = (int) pv (PID::pitchRange);
    s.gateProb   = push (pv (PID::gateProb), x, 0.4f);
    s.variation  = push (pv (PID::variation), x, 1.0f);
    s.backbone   = cl (pv (PID::backbone) * (1.0f - 0.6f * juce::jmax (0.0f, x - 0.5f) * 2.0f));
    s.humanize   = pv (PID::humanize);
    return s;
}

//==============================================================================
void JSliceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // ---- host transport
    double hBpm = 0.0, hPpq = 0.0;
    bool hPlay = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) hBpm = *b;
            hPlay = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) hPpq = *q;
        }
    }
    hostPlaying.store (hPlay);
    hostBpm.store (hBpm);

    // SYNC on  : tempo and bar position follow the DAW (the break is re-pitched / stretched to the host tempo)
    // SYNC off : J-Slice runs on its own tempo, starting when the DAW or the PLAY button starts
    const bool sync = pv (PID::syncOn) > 0.5f;
    Transport t;
    t.bpm = (sync && hBpm > 1.0) ? hBpm : (double) pv (PID::freeBpm);
    lastBpm.store (t.bpm);

    const bool internal = internalPlay.load();
    if (sync && hPlay)
    {
        t.playing = true;
        t.ppq = hPpq;
        internalRunning = false;
        internalPpq = 0.0;
    }
    else if (internal || (! sync && hPlay))
    {
        if (! internalRunning) { internalRunning = true; internalPpq = 0.0; }
        t.playing = true;
        t.ppq = internalPpq;
        internalPpq += buffer.getNumSamples() / getSampleRate() * t.bpm / 60.0;
    }
    else
    {
        internalRunning = false;
        internalPpq = 0.0;
    }
    running.store (t.playing);

    engine.process (buffer, midi, t, currentEngineParams());
    midi.clear();
}


//==============================================================================
void JSliceProcessor::parameterChanged (const juce::String& id, float value)
{
    if (id == PID::genTrig || id == PID::mutTrig || id == PID::fillTrig)
    {
        if (value < 0.5f) return;
        if (id == PID::genTrig)  genRequests.fetch_add (1);
        if (id == PID::mutTrig)  mutRequests.fetch_add (1);
        if (id == PID::fillTrig) { fillRequests.fetch_add (1); engine.requestFill(); }
        return;
    }
    // generator parameter : schedule a live re-generation (handled on the message thread)
    regenRequestTime.store (juce::Time::getMillisecondCounter());
    regenPending.store (true);
}

void JSliceProcessor::timerCallback()
{
    updateSlots();
    if (genRequests.exchange (0) > 0) generate();
    if (mutRequests.exchange (0) > 0) mutate();
    fillRequests.store (0);

    if (regenPending.load() && juce::Time::getMillisecondCounter() - regenRequestTime.load() > 40)
    {
        regenPending.store (false);
        if (! restoringState) regenerateLive();
    }

    const int g = engine.midiGenerate.load(), m = engine.midiMutate.load(), u = engine.midiUndo.load(), s = engine.midiSeqToggle.load();
    if (g != lastMidiGen) { lastMidiGen = g; generate(); }
    if (m != lastMidiMut) { lastMidiMut = m; mutate(); }
    if (u != lastMidiUndo) { lastMidiUndo = u; undo(); }
    if (s != lastMidiSeq)
    {
        lastMidiSeq = s;
        if (auto* p = apvts.getParameter (PID::seqOn))
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
    }
}

//==============================================================================
AudioData::Ptr JSliceProcessor::readAudio (const juce::File& f, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) { error = "Unsupported or unreadable file: " + f.getFileName(); return nullptr; }
    if (r->sampleRate <= 0 || r->lengthInSamples <= 0) { error = "Empty audio file"; return nullptr; }

    const auto maxLen = (juce::int64) (r->sampleRate * 60.0);
    const int n = (int) juce::jmin (r->lengthInSamples, maxLen);
    AudioData::Ptr a = new AudioData();
    a->buffer.setSize (2, n);
    a->buffer.clear();
    r->read (&a->buffer, 0, n, 0, true, r->numChannels > 1);
    if (r->numChannels == 1) a->buffer.copyFrom (1, 0, a->buffer, 0, 0, n);
    a->sampleRate = r->sampleRate;
    a->file = f;
    a->name = f.getFileNameWithoutExtension();
    return a;
}

void JSliceProcessor::setKit (int slot, Kit::Ptr k)
{
    {
        const juce::ScopedLock sl (stateLock);
        breaks[(size_t) slot].kit = k;
    }
    engine.setKit (slot, k);
}

bool JSliceProcessor::loadFile (int slot, const juce::File& f, juce::String& error)
{
    slot = juce::jlimit (0, kNumBreaks - 1, slot);
    auto a = readAudio (f, error);
    if (a == nullptr) return false;
    setAudio (slot, a, keepPatternOnLoad());
    globalSettings->setValue ("lastDir", f.getParentDirectory().getFullPathName());
    globalSettings->saveIfNeeded();
    return true;
}

void JSliceProcessor::setAudio (int slot, AudioData::Ptr audio, bool keepPattern)
{
    auto& sl = breaks[(size_t) slot];
    sl.liveBaseKit = nullptr;
    Kit::Ptr oldKit = sl.kit;
    sl.analysis = Analyzer::analyze (audio->buffer, audio->sampleRate);

    Kit::Ptr k = new Kit();
    k->audio = audio;
    k->bpm = sl.analysis.bpm;
    k->bars = sl.analysis.bars;
    k->gridOrigin = sl.analysis.gridOrigin;
    k->slices = Analyzer::makeSlices (sl.analysis, audio->buffer, audio->sampleRate, k->bpm, k->bars, k->gridOrigin, sl.slicer);
    Analyzer::classify (k->slices, audio->buffer, audio->sampleRate, k->bpm, k->gridOrigin);
    setKit (slot, k);

    bool hasHits = false;
    for (auto& s : pattern.steps) hasHits |= s.on && s.src == slot;
    if (keepPattern && hasHits && oldKit != nullptr && ! oldKit->slices.empty())
        setPattern (Generator::remap (pattern, *oldKit, *k, slot), true);
    else
        generate();
    sendChangeMessage();
}

juce::File JSliceProcessor::getCurrentFile (int slot) const
{
    auto k = getKit (slot);
    return (k != nullptr && k->audio != nullptr) ? k->audio->file : juce::File();
}

juce::File JSliceProcessor::getSiblingFile (int slot, int delta) const
{
    const auto cur = getCurrentFile (slot);
    if (! cur.existsAsFile()) return {};
    auto files = cur.getParentDirectory().findChildFiles (juce::File::findFiles, false, getSupportedWildcard());
    if (files.isEmpty()) return {};
    files.sort();
    int idx = files.indexOf (cur);
    if (idx < 0) idx = 0;
    idx = (idx + delta + files.size()) % files.size();
    return files[idx];
}

void JSliceProcessor::rebuildSlices (int slot)
{
    auto& sl = breaks[(size_t) slot];
    if (sl.kit == nullptr) return;
    auto& a = *sl.kit->audio;
    auto slices = Analyzer::makeSlices (sl.analysis, a.buffer, a.sampleRate, sl.kit->bpm, sl.kit->bars, sl.kit->gridOrigin, sl.slicer);
    Analyzer::classify (slices, a.buffer, a.sampleRate, sl.kit->bpm, sl.kit->gridOrigin);
    const auto now = juce::Time::getMillisecondCounter();
    const bool newGesture = sl.liveBaseKit == nullptr || now - sl.liveBaseTime > 700;
    if (newGesture)
    {
        sl.liveBaseKit = sl.kit;
        sl.liveBasePattern = pattern;
    }
    sl.liveBaseTime = now;
    setKit (slot, sl.kit->cloneWithSlices (std::move (slices)));
    setPattern (Generator::remap (sl.liveBasePattern, *sl.liveBaseKit, *sl.kit, slot), newGesture);
    sendChangeMessage();
}

void JSliceProcessor::setSlicerSettings (int slot, const SlicerSettings& s)
{
    slot = juce::jlimit (0, kNumBreaks - 1, slot);
    breaks[(size_t) slot].slicer = s;
    rebuildSlices (slot);
}

void JSliceProcessor::setBreakTempo (int slot, double bpm, int bars, bool reslice)
{
    auto& sl = breaks[(size_t) juce::jlimit (0, kNumBreaks - 1, slot)];
    if (sl.kit == nullptr) return;
    Kit::Ptr k = sl.kit->cloneWithSlices (sl.kit->slices);
    k->bpm = juce::jlimit (30.0, 400.0, bpm);
    k->bars = juce::jlimit (1, 64, bars);
    Analyzer::finaliseSlices (k->slices, k->length(), k->audio->sampleRate, k->bpm, k->gridOrigin);
    setKit (slot, k);
    if (reslice) rebuildSlices (slot);
    else sendChangeMessage();
}

void JSliceProcessor::redetectTempo (int slot)
{
    auto& sl = breaks[(size_t) juce::jlimit (0, kNumBreaks - 1, slot)];
    if (sl.kit == nullptr) return;
    Kit::Ptr k = sl.kit->cloneWithSlices (sl.kit->slices);
    k->bpm = sl.analysis.bpm;
    k->bars = sl.analysis.bars;
    k->gridOrigin = sl.analysis.gridOrigin;
    setKit (slot, k);
    rebuildSlices (slot);
}

void JSliceProcessor::updateSlices (int slot, std::vector<Slice> slices, bool reclassify)
{
    slot = juce::jlimit (0, kNumBreaks - 1, slot);
    auto& sl = breaks[(size_t) slot];
    sl.liveBaseKit = nullptr;
    if (sl.kit == nullptr) return;
    auto& a = *sl.kit->audio;
    Analyzer::finaliseSlices (slices, a.buffer.getNumSamples(), a.sampleRate, sl.kit->bpm, sl.kit->gridOrigin);
    if (slices.empty()) return;
    if (reclassify) Analyzer::classify (slices, a.buffer, a.sampleRate, sl.kit->bpm, sl.kit->gridOrigin);
    const int n = (int) slices.size();
    setKit (slot, sl.kit->cloneWithSlices (std::move (slices)));
    bool changed = false;
    auto p = pattern;
    for (auto& s : p.steps) if (s.src == slot && s.slice >= n) { s.slice = (int16_t) (n - 1); changed = true; }
    for (auto& s : p.fill)  if (s.src == slot && s.slice >= n) { s.slice = (int16_t) (n - 1); changed = true; }
    if (changed) setPattern (p, false);
    sendChangeMessage();
}

//==============================================================================
void JSliceProcessor::setPattern (const Pattern& p, bool undoable)
{
    if (undoable && ! (p == pattern))
    {
        undoStack.push_back (pattern);
        if (undoStack.size() > 64) undoStack.erase (undoStack.begin());
        redoStack.clear();
    }
    {
        const juce::ScopedLock sl (stateLock);
        pattern = p;
        if (pattern.length <= 0) pattern.length = juce::jlimit (1, kMaxSteps, (int) pv (PID::steps));
        auto& cur = slots[(size_t) editSlot];
        cur.pattern = pattern;
        for (int k = 0; k < kNumBreaks; ++k) cur.madeFor[(size_t) k] = breaks[(size_t) k].kit;
    }
    if (editSlot == playSlot)
        engine.setPattern (pattern, pv (PID::quantize) > 0.5f && hostPlaying.load());
    else if (editSlot == queuedSlot)
        engine.queuePattern (pattern);
    sendChangeMessage();
}

//==============================================================================
// Pattern slots
namespace
{
    // generator settings stored with each slot
    const char* const kSlotParams[] = { PID::style, PID::complexity, PID::energy, PID::chaos, PID::density, PID::backbone,
                                        PID::ghosts, PID::hats, PID::chop, PID::cut16, PID::rolls, PID::stutter, PID::reverse,
                                        PID::pitchProb, PID::pitchRange, PID::gateProb, PID::variation, PID::fillAmt,
                                        PID::maxRepeats, PID::humanize, PID::swing, PID::fillOn, PID::fillEvery,
                                        PID::duoMode, PID::bShare };

    juce::String setToString (const std::set<int>& s)
    {
        juce::String out;
        for (int i : s) out << i << ",";
        return out;
    }

    std::set<int> setFromString (const juce::String& str)
    {
        std::set<int> out;
        for (auto& t : juce::StringArray::fromTokens (str, ",", ""))
            if (t.isNotEmpty()) out.insert (t.getIntValue());
        return out;
    }
}

bool JSliceProcessor::slotUsed (int i) const
{
    if (i < 0 || i >= kSlots) return false;
    const auto& p = i == editSlot ? pattern : slots[(size_t) i].pattern;
    for (auto& st : p.steps) if (st.on) return true;
    return false;
}

void JSliceProcessor::captureSettings (Slot& s) const
{
    for (auto* id : kSlotParams)
        if (auto* prm = apvts.getParameter (id))
            s.settings.set (id, prm->convertFrom0to1 (prm->getValue()));
}

void JSliceProcessor::applySettings (const Slot& s)
{
    for (auto* id : kSlotParams)
        if (auto* v = s.settings.getVarPointer (id))
            if (auto* prm = apvts.getParameter (id))
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) *v));
    regenPending.store (false);    // loading a slot's settings must not re-generate its pattern
}

Pattern JSliceProcessor::remapToBreaks (Pattern p, const std::array<Kit::Ptr, kNumBreaks>& madeFor) const
{
    const int len = p.length;
    for (int k = 0; k < kNumBreaks; ++k)
    {
        const auto& now = breaks[(size_t) k].kit;
        const auto& was = madeFor[(size_t) k];
        if (now != nullptr && was != nullptr && was != now && ! was->slices.empty())
            p = Generator::remap (p, *was, *now, k);
    }
    p.length = len;
    return p;
}

Pattern JSliceProcessor::slotPattern (int i)
{
    auto& sl = slots[(size_t) i];
    sl.pattern = remapToBreaks (sl.pattern, sl.madeFor);
    for (int k = 0; k < kNumBreaks; ++k)
        if (breaks[(size_t) k].kit != nullptr) sl.madeFor[(size_t) k] = breaks[(size_t) k].kit;
    if (sl.pattern.length <= 0) sl.pattern.length = juce::jlimit (1, kMaxSteps, (int) pv (PID::steps));
    return sl.pattern;
}

void JSliceProcessor::queueSlot (int i)
{
    queuedSlot = i;
    engine.queuePattern (slotPattern (i));
}

void JSliceProcessor::selectSlot (int i)
{
    i = juce::jlimit (0, kSlots - 1, i);
    if (i == editSlot && queuedSlot < 0) return;
    captureSettings (slots[(size_t) editSlot]);
    slots[(size_t) editSlot].pattern = pattern;
    slots[(size_t) editSlot].manual = manualSteps;
    editSlot = i;
    applySettings (slots[(size_t) i]);
    {
        const juce::ScopedLock sl (stateLock);
        pattern = slotPattern (i);
    }
    manualSteps = slots[(size_t) i].manual;
    if (auto* prm = apvts.getParameter (PID::steps))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) pattern.length));
    undoStack.clear();
    redoStack.clear();
    if (! chainOn)
    {
        if (running.load())
        {
            if (i == playSlot) { queuedSlot = -1; engine.cancelQueue(); }   // back to the playing one: cancel the change
            else queueSlot (i);
        }
        else { playSlot = i; queuedSlot = -1; engine.setPattern (pattern, false); }
    }
    sendChangeMessage();
}

void JSliceProcessor::copySlot()
{
    slotClip = std::make_unique<Slot> (slots[(size_t) editSlot]);
    slotClip->pattern = pattern;
    slotClip->manual = manualSteps;
    for (int k = 0; k < kNumBreaks; ++k) slotClip->madeFor[(size_t) k] = breaks[(size_t) k].kit;
    captureSettings (*slotClip);
    sendChangeMessage();
}

void JSliceProcessor::pasteSlot()
{
    if (slotClip == nullptr) return;
    slots[(size_t) editSlot].settings = slotClip->settings;
    applySettings (*slotClip);
    auto p = remapToBreaks (slotClip->pattern, slotClip->madeFor);
    manualSteps = slotClip->manual;
    setPattern (p, true);
    if (auto* prm = apvts.getParameter (PID::steps))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) pattern.length));
}

void JSliceProcessor::clearSlot()
{
    Pattern p;
    p.length = pattern.length;
    p.seed = pattern.seed;
    manualSteps.clear();
    for (int i = 0; i < kMaxSteps + kStepsPerBar; ++i) manualSteps.insert (i);   // an emptied slot stays empty when the macros move
    setPattern (p, true);
}

void JSliceProcessor::setChain (std::vector<ChainItem> c)
{
    for (auto& it : c) { it.slot = juce::jlimit (0, kSlots - 1, it.slot); it.repeats = juce::jlimit (1, 16, it.repeats); }
    chain = std::move (c);
    if (chain.empty()) chainOn = false;
    chainPos = juce::jlimit (0, juce::jmax (0, (int) chain.size() - 1), chainPos);
    sendChangeMessage();
}

void JSliceProcessor::setChainOn (bool on)
{
    chainOn = on && ! chain.empty();
    chainPos = 0; chainReps = 0; lastLoop = -1;
    if (chainOn)
    {
        const int first = chain[0].slot;
        if (running.load()) queueSlot (first);
        else
        {
            selectSlot (first);
            playSlot = first; queuedSlot = -1; engine.setPattern (pattern, false);
        }
    }
    else if (queuedSlot >= 0) { queuedSlot = -1; engine.cancelQueue(); }
    sendChangeMessage();
}

void JSliceProcessor::updateSlots()
{
    // the pattern length knob belongs to the edited slot
    const int st = juce::jlimit (1, kMaxSteps, (int) pv (PID::steps));
    if (pattern.length != st) { auto p = pattern; p.length = st; setPattern (p, false); }

    const int sw = engine.swapCount.load();
    if (sw != lastSwap)
    {
        lastSwap = sw;
        if (queuedSlot >= 0)
        {
            playSlot = queuedSlot;
            queuedSlot = -1;
            chainReps = 0;
            lastLoop = -1;
            if (chainOn && playSlot != editSlot) selectSlot (playSlot);   // in a chain, the view follows the music
            sendChangeMessage();
        }
    }
    if (! chainOn || chain.empty() || ! running.load()) return;
    const auto loop = (juce::int64) engine.loopIndex.load();
    if (loop != lastLoop)
    {
        if (lastLoop >= 0) ++chainReps;
        lastLoop = loop;
    }
    // during the last repeat of this chain entry, queue the next one (it starts when this one ends)
    const auto& cur = chain[(size_t) juce::jlimit (0, (int) chain.size() - 1, chainPos)];
    if (queuedSlot < 0 && chainReps >= cur.repeats - 1)
    {
        chainPos = (chainPos + 1) % (int) chain.size();
        queueSlot (chain[(size_t) chainPos].slot);
        sendChangeMessage();
    }
}

void JSliceProcessor::modifyStep (int index, const std::function<void (Step&)>& fn, bool undoable)
{
    auto p = pattern;
    if (index >= 0 && index < kMaxSteps) fn (p.steps[(size_t) index]);
    else if (index >= kMaxSteps && index < kMaxSteps + kStepsPerBar) fn (p.fill[(size_t) (index - kMaxSteps)]);
    else return;
    manualSteps.insert (index);
    setPattern (p, undoable);
}

Pattern JSliceProcessor::buildPattern (uint32_t seed) const
{
    const auto gs = currentGenSettings();
    const Kit* ka = breaks[0].kit.get();
    const Kit* kb = isBreakBActive() ? breaks[1].kit.get() : nullptr;
    Pattern out;
    if (ka != nullptr && ! ka->slices.empty())
    {
        auto a = Generator::generate (*ka, gs, seed, nullptr);
        for (auto& st : a.steps) st.src = 0;
        for (auto& st : a.fill)  st.src = 0;
        if (kb != nullptr && ! kb->slices.empty())
        {
            const int mode = (int) pv (PID::duoMode);
            auto b = Generator::generate (*kb, Generator::settingsForB (gs, mode, pv (PID::bShare)), seed * 31u + 7u, nullptr);
            out = Generator::combine (a, b, mode, pv (PID::bShare), seed);
        }
        else out = a;
    }
    else if (kb != nullptr && ! kb->slices.empty())
    {
        out = Generator::generate (*kb, gs, seed * 31u + 7u, nullptr);
        for (auto& st : out.steps) st.src = 1;
        for (auto& st : out.fill)  st.src = 1;
    }
    out.seed = seed;
    return out;
}

void JSliceProcessor::generate (std::optional<uint32_t> seed)
{
    if (breaks[0].kit == nullptr && breaks[1].kit == nullptr) return;
    const uint32_t s = seed.has_value() ? *seed : (uint32_t) juce::Random::getSystemRandom().nextInt (999999) + 1;
    auto p = buildPattern (s);
    p.length = pattern.length;
    for (size_t i = 0; i < p.steps.size(); ++i) if (pattern.steps[i].locked) p.steps[i] = pattern.steps[i];
    for (size_t i = 0; i < p.fill.size(); ++i)  if (pattern.fill[i].locked)  p.fill[i] = pattern.fill[i];
    manualSteps.clear();
    setPattern (p, true);
}

void JSliceProcessor::regenerateLive()
{
    if (breaks[0].kit == nullptr && breaks[1].kit == nullptr) return;
    auto p = buildPattern (pattern.seed);
    p.length = pattern.length;
    // keep locked steps and the steps edited by hand
    for (size_t i = 0; i < p.steps.size(); ++i)
        if (pattern.steps[i].locked || manualSteps.count ((int) i) > 0) p.steps[i] = pattern.steps[i];
    for (size_t i = 0; i < p.fill.size(); ++i)
        if (pattern.fill[i].locked || manualSteps.count (kMaxSteps + (int) i) > 0) p.fill[i] = pattern.fill[i];
    const auto now = juce::Time::getMillisecondCounter();
    const bool newGesture = now - regenGestureTime > 800;
    regenGestureTime = now;
    setPattern (p, newGesture);
}

void JSliceProcessor::mutate()
{
    if (breaks[0].kit == nullptr && breaks[1].kit == nullptr) return;
    auto p = pattern;
    const Kit* kits[kNumBreaks] = { breaks[0].kit.get(), breaks[1].kit.get() };
    Generator::mutateDuo (p, kits, currentGenSettings(), (uint32_t) juce::Random::getSystemRandom().nextInt (999999) + 1);
    setPattern (p, true);
}

void JSliceProcessor::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back (pattern);
    auto p = undoStack.back();
    undoStack.pop_back();
    setPattern (p, false);
}

void JSliceProcessor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (pattern);
    auto p = redoStack.back();
    redoStack.pop_back();
    setPattern (p, false);
}

void JSliceProcessor::clearLocks()
{
    auto p = pattern;
    for (auto& s : p.steps) s.locked = false;
    for (auto& s : p.fill)  s.locked = false;
    setPattern (p, true);
}

void JSliceProcessor::clearStepFx (uint8_t mask)
{
    auto p = pattern;
    for (auto& s : p.steps) s.fx = (uint8_t) (s.fx & ~mask);
    for (auto& s : p.fill)  s.fx = (uint8_t) (s.fx & ~mask);
    setPattern (p, true);
}

void JSliceProcessor::applyStyle (int index)
{
    const auto& styles = getStyles();
    index = juce::jlimit (0, (int) styles.size() - 1, index);
    const auto& st = styles[(size_t) index];
    const auto& d = st.defaults;

    auto set = [this] (const char* id, float plainValue)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    };
    set (PID::style, (float) index);
    set (PID::density, d.density);     set (PID::backbone, d.backbone);   set (PID::ghosts, d.ghosts);
    set (PID::hats, d.hats);           set (PID::chop, d.chop);           set (PID::cut16, d.cut16);
    set (PID::rolls, d.rolls);         set (PID::stutter, d.stutter);     set (PID::reverse, d.reverse);
    set (PID::pitchProb, d.pitchProb); set (PID::pitchRange, (float) d.pitchRange);
    set (PID::gateProb, d.gateProb);   set (PID::variation, d.variation); set (PID::fillAmt, d.fillAmount);
    set (PID::maxRepeats, (float) d.maxRepeats); set (PID::humanize, d.humanize);
    set (PID::tailMode, (float) st.tailMode);
    set (PID::tempoMode, (float) st.tempoMode);
    set (PID::swing, st.swing);
    regenPending.store (false);
    generate();
}

//==============================================================================
juce::File JSliceProcessor::exportMidi()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Kinetik Jungle");
    dir.createDirectory();
    const auto ep = currentEngineParams();
    const auto name = juce::String ("KJNG-2 ") + juce::String ((int) std::round (lastBpm.load())) + "bpm " + juce::String ((int) pattern.seed);
    auto f = dir.getChildFile (juce::File::createLegalFileName (name) + ".mid");
    Exporter::writeMidi (pattern, ep, Exporter::exportBars (ep), lastBpm.load(), f);
    return f;
}

juce::File JSliceProcessor::exportAudio()
{
    auto ka = breaks[0].kit;
    if (ka == nullptr) return {};
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Kinetik Jungle");
    dir.createDirectory();
    const auto ep = currentEngineParams();
    const auto name = juce::String ("KJNG-2 ") + (ka->audio != nullptr ? ka->audio->name : juce::String())
                      + " " + juce::String ((int) std::round (lastBpm.load())) + "bpm " + juce::String ((int) pattern.seed);
    auto f = dir.getChildFile (juce::File::createLegalFileName (name) + ".wav");
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    Exporter::renderAudio (ka, pattern, ep, Exporter::exportBars (ep), lastBpm.load(), sr, f, breaks[1].kit);
    return f;
}

//==============================================================================
juce::StringArray JSliceProcessor::getFavourites() const
{
    auto s = juce::StringArray::fromLines (globalSettings->getValue ("favourites"));
    s.removeEmptyStrings();
    return s;
}

void JSliceProcessor::setFavourites (const juce::StringArray& f)
{
    globalSettings->setValue ("favourites", f.joinIntoString ("\n"));
    globalSettings->saveIfNeeded();
}

//==============================================================================
namespace
{
    juce::ValueTree writeSlot (const Kit* kit, const SlicerSettings& slicer, int slot)
    {
        juce::ValueTree st ("BREAK");
        st.setProperty ("slot", slot, nullptr);
        st.setProperty ("slMode", slicer.mode, nullptr);
        st.setProperty ("slSens", slicer.sensitivity, nullptr);
        st.setProperty ("slGrid", slicer.gridDivision, nullptr);
        st.setProperty ("slPre", slicer.prerollMs, nullptr);
        if (kit == nullptr || kit->audio == nullptr) return st;

        st.setProperty ("file", kit->audio->file.getFullPathName(), nullptr);
        st.setProperty ("name", kit->audio->name, nullptr);
        st.setProperty ("bpm", kit->bpm, nullptr);
        st.setProperty ("bars", kit->bars, nullptr);
        st.setProperty ("origin", (juce::int64) kit->gridOrigin, nullptr);

        juce::ValueTree sls ("SLICES");
        for (auto& s : kit->slices)
        {
            juce::ValueTree c ("S");
            c.setProperty ("start", (juce::int64) s.start, nullptr);
            c.setProperty ("cls", (int) s.cls, nullptr);
            c.setProperty ("user", s.userClass, nullptr);
            c.setProperty ("str", s.strength, nullptr);
            sls.appendChild (c, nullptr);
        }
        st.appendChild (sls, nullptr);

        // embed the audio (FLAC) so the project still works if the file is moved or on another computer
        const auto& b = kit->audio->buffer;
        if (b.getNumSamples() < (int) (kit->audio->sampleRate * 60.0))
        {
            juce::MemoryBlock mb;
            juce::FlacAudioFormat flac;
            std::unique_ptr<juce::OutputStream> mos = std::make_unique<juce::MemoryOutputStream> (mb, false);
            auto w = flac.createWriterFor (mos, juce::AudioFormatWriterOptions{}.withSampleRate (kit->audio->sampleRate)
                                                                              .withNumChannels (2).withBitsPerSample (24));
            if (w != nullptr)
            {
                w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                w.reset();
                st.setProperty ("audio", mb, nullptr);
            }
        }
        return st;
    }
}

void JSliceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree st ("KJNG");
    st.setProperty ("version", 2, nullptr);
    st.appendChild (apvts.copyState(), nullptr);

    const juce::ScopedLock sl (stateLock);
    st.setProperty ("pattern", pattern.toString(), nullptr);
    st.setProperty ("manual", setToString (manualSteps), nullptr);
    // pattern slots (the edited one takes its settings from the parameters, saved above)
    st.setProperty ("editSlot", editSlot, nullptr);
    st.setProperty ("chainOn", chainOn, nullptr);
    {
        juce::String ch;
        for (auto& it : chain) ch << it.slot << ":" << it.repeats << ",";
        st.setProperty ("chain", ch, nullptr);
    }
    juce::ValueTree slotsTree ("SLOTS");
    for (int i = 0; i < kSlots; ++i)
    {
        if (i == editSlot) continue;
        const auto& sl = slots[(size_t) i];
        bool used = false;
        for (auto& x : sl.pattern.steps) used |= x.on;
        if (! used && sl.settings.isEmpty()) continue;
        juce::ValueTree c ("SLOT");
        c.setProperty ("index", i, nullptr);
        c.setProperty ("pattern", sl.pattern.toString(), nullptr);
        c.setProperty ("manual", setToString (sl.manual), nullptr);
        for (auto& nv : sl.settings) c.setProperty ("s_" + nv.name.toString(), nv.value, nullptr);
        slotsTree.appendChild (c, nullptr);
    }
    st.appendChild (slotsTree, nullptr);
    for (int k = 0; k < kNumBreaks; ++k)
        st.appendChild (writeSlot (breaks[(size_t) k].kit.get(), breaks[(size_t) k].slicer, k), nullptr);

    juce::MemoryOutputStream os (destData, false);
    st.writeToStream (os);
}

void JSliceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto st = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! st.isValid() || ! (st.hasType ("KJNG") || st.hasType ("JSLICE"))) return;

    auto loadSlot = [this] (int slot, const juce::ValueTree& b)
    {
        auto& sl = breaks[(size_t) slot];
        sl.slicer.mode = b.getProperty ("slMode", 0);
        sl.slicer.sensitivity = b.getProperty ("slSens", 0.5f);
        sl.slicer.gridDivision = b.getProperty ("slGrid", 16);
        sl.slicer.prerollMs = b.getProperty ("slPre", 1.5f);
        sl.liveBaseKit = nullptr;

        AudioData::Ptr audio;
        const juce::File f (b.getProperty ("file").toString());
        juce::String err;
        if (f.existsAsFile()) audio = readAudio (f, err);
        if (audio == nullptr)
        {
            if (auto* mb = b.getProperty ("audio").getBinaryData())
            {
                juce::FlacAudioFormat flac;
                std::unique_ptr<juce::AudioFormatReader> r (flac.createReaderFor (new juce::MemoryInputStream (*mb, false), true));
                if (r != nullptr)
                {
                    audio = new AudioData();
                    audio->buffer.setSize (2, (int) r->lengthInSamples);
                    r->read (&audio->buffer, 0, (int) r->lengthInSamples, 0, true, true);
                    audio->sampleRate = r->sampleRate;
                    audio->file = f;
                    audio->name = b.getProperty ("name").toString();
                }
            }
        }
        if (audio == nullptr) { setKit (slot, nullptr); return; }

        sl.analysis = Analyzer::analyze (audio->buffer, audio->sampleRate);
        Kit::Ptr k = new Kit();
        k->audio = audio;
        k->bpm = b.getProperty ("bpm", sl.analysis.bpm);
        k->bars = b.getProperty ("bars", sl.analysis.bars);
        k->gridOrigin = (int64_t) (juce::int64) b.getProperty ("origin", (juce::int64) sl.analysis.gridOrigin);

        std::vector<Slice> slices;
        auto sls = b.getChildWithName ("SLICES");
        for (auto c : sls)
        {
            Slice s;
            s.start = (int64_t) (juce::int64) c.getProperty ("start");
            s.cls = (HitClass) juce::jlimit (0, kNumClasses - 1, (int) c.getProperty ("cls"));
            s.userClass = c.getProperty ("user");
            s.strength = c.getProperty ("str", 0.5f);
            slices.push_back (s);
        }
        if (slices.empty())
            slices = Analyzer::makeSlices (sl.analysis, audio->buffer, audio->sampleRate, k->bpm, k->bars, k->gridOrigin, sl.slicer);
        Analyzer::finaliseSlices (slices, audio->buffer.getNumSamples(), audio->sampleRate, k->bpm, k->gridOrigin);
        auto saved = slices;
        Analyzer::classify (slices, audio->buffer, audio->sampleRate, k->bpm, k->gridOrigin);
        if (sls.isValid() && sls.getNumChildren() > 0)
            for (size_t i = 0; i < slices.size() && i < saved.size(); ++i)
                slices[i].cls = saved[i].cls;
        k->slices = std::move (slices);
        setKit (slot, k);
    };

    auto apply = [this, st, loadSlot]
    {
        restoringState = true;
        auto params = st.getChildWithName (apvts.state.getType());
        if (params.isValid()) apvts.replaceState (params);

        if (st.hasType ("JSLICE"))
        {
            loadSlot (0, st);                // version 1 : one break stored at the root
            setKit (1, nullptr);
        }
        else
        {
            for (int k = 0; k < kNumBreaks; ++k) setKit (k, nullptr);
            for (auto b : st)
                if (b.hasType ("BREAK"))
                    loadSlot (juce::jlimit (0, kNumBreaks - 1, (int) b.getProperty ("slot", 0)), b);
        }

        undoStack.clear();
        redoStack.clear();
        manualSteps = setFromString (st.getProperty ("manual").toString());

        auto clampStep = [this] (Step& s)
        {
            auto k = breaks[(size_t) juce::jlimit (0, kNumBreaks - 1, (int) s.src)].kit;
            const int n = k != nullptr ? (int) k->slices.size() : 0;
            if (n == 0) { if (s.on) s = Step(); return; }
            s.slice = (int16_t) juce::jlimit (0, n - 1, (int) s.slice);
        };
        auto p = Pattern::fromString (st.getProperty ("pattern").toString());
        for (auto& s : p.steps) clampStep (s);
        for (auto& s : p.fill)  clampStep (s);

        // pattern slots and chain
        for (auto& sl : slots) sl = Slot();
        editSlot = juce::jlimit (0, kSlots - 1, (int) st.getProperty ("editSlot", 0));
        playSlot = editSlot; queuedSlot = -1;
        engine.cancelQueue();
        for (auto c : st.getChildWithName ("SLOTS"))
        {
            const int i = c.getProperty ("index", -1);
            if (i < 0 || i >= kSlots || i == editSlot) continue;
            auto& sl = slots[(size_t) i];
            sl.pattern = Pattern::fromString (c.getProperty ("pattern").toString());
            for (auto& x : sl.pattern.steps) clampStep (x);
            for (auto& x : sl.pattern.fill)  clampStep (x);
            sl.manual = setFromString (c.getProperty ("manual").toString());
            for (int k = 0; k < kNumBreaks; ++k) sl.madeFor[(size_t) k] = breaks[(size_t) k].kit;
            for (int k = 0; k < c.getNumProperties(); ++k)
            {
                const auto name = c.getPropertyName (k).toString();
                if (name.startsWith ("s_")) sl.settings.set (name.substring (2), c.getProperty (name));
            }
        }
        chain.clear();
        for (auto& tok : juce::StringArray::fromTokens (st.getProperty ("chain").toString(), ",", ""))
            if (tok.containsChar (':'))
                chain.push_back ({ juce::jlimit (0, kSlots - 1, tok.upToFirstOccurrenceOf (":", false, false).getIntValue()),
                                   juce::jlimit (1, 16, tok.fromFirstOccurrenceOf (":", false, false).getIntValue()) });
        chainOn = (bool) st.getProperty ("chainOn", false) && ! chain.empty();
        chainPos = 0; chainReps = 0; lastLoop = -1;

        setPattern (p, false);
        regenPending.store (false);
        restoringState = false;
        sendChangeMessage();
    };

    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread())
        apply();
    else
    {
        std::weak_ptr<bool> weak = alive;
        juce::MessageManager::callAsync ([weak, apply]
        {
            if (auto a = weak.lock()) if (*a) apply();
        });
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JSliceProcessor();
}
