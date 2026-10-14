#include "PluginEditor.h"

using namespace ui;

namespace
{
    constexpr int kW = 1180, kH = 942;

    juce::String pct (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; }
    juce::String utf8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
}

//==============================================================================
void Page::paint (juce::Graphics& g)
{
    for (auto& gr : groups)
    {
        g.setColour (pal().inset.withAlpha (0.6f));
        g.fillRoundedRectangle (gr.r.toFloat(), 5.0f);
        g.setColour (pal().line);
        g.drawRoundedRectangle (gr.r.toFloat().reduced (0.5f), 5.0f, 1.0f);
        if (gr.title.isNotEmpty())
        {
            g.setFont (mono (11.0f));
            g.setColour (pal().dim);
            g.drawText (gr.title.toUpperCase(), gr.r.getX() + 8, gr.r.getY() + 5, gr.r.getWidth() - 16, 16, juce::Justification::centredLeft);
        }
    }
}

//==============================================================================
BreakCard::BreakCard (JSliceProcessor& p, int s) : proc (p), slot (s)
{
    prevBtn.setTooltip ("Previous file in the same folder");
    nextBtn.setTooltip ("Next file in the same folder");
    prevBtn.onClick = [this] { if (onSibling) onSibling (-1); };
    nextBtn.onClick = [this] { if (onSibling) onSibling (1); };
    addAndMakeVisible (prevBtn);
    addAndMakeVisible (nextBtn);
    if (slot == 1)
    {
        onBtn = std::make_unique<ParamToggle> (p.apvts, PID::bOn, "On");
        onBtn->setTooltip ("Break B on / off. On : a second generator places B hits in the gaps of A (see DUO above the lanes).");
        addAndMakeVisible (*onBtn);
    }
    setTooltip (slot == 0 ? "Break A : click to edit its slices with the SLICER row. Drop a file on the waveform to replace it."
                          : "Break B : click to edit its slices with the SLICER row. Drop a file on the waveform to load it.");
}

void BreakCard::resized()
{
    const int w = getWidth(), h = getHeight();
    prevBtn.setBounds (w - 54, h - 28, 24, 22);
    nextBtn.setBounds (w - 28, h - 28, 24, 22);
    if (onBtn != nullptr) onBtn->setBounds (w - 50, 5, 46, 20);
}

void BreakCard::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto& pl = pal();
    const auto col = breakColour (slot);
    auto k = proc.getKit (slot);
    const bool on = slot == 0 || proc.apvts.getRawParameterValue (PID::bOn)->load() > 0.5f;
    auto r = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (active ? col : pl.raised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (active ? col : col.withAlpha (on ? 0.6f : 0.25f));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);

    const auto ink = active ? pl.onAccent : pl.text;
    g.setFont (serif (50.0f));
    g.setColour (active ? pl.onAccent : col.withAlpha (on ? 1.0f : 0.4f));
    g.drawText (slot == 0 ? "A" : "B", 8, 2, 40, getHeight() - 4, Justification::centredLeft);

    g.setFont (mono (10.0f, true, 0.02f));
    g.setColour (active ? pl.onAccent.withAlpha (0.8f) : pl.dim);
    const int infoW = getWidth() - 52 - (onBtn != nullptr ? 52 : 0);
    if (k != nullptr)
        g.drawFittedText (String (k->bpm, 1) + " BPM" + utf8 (" \xc2\xb7 ") + String (k->bars) + (k->bars > 1 ? " BARS" : " BAR"),
                          48, 6, infoW, 14, Justification::centredLeft, 1, 0.8f);
    else
        g.drawText (slot == 0 ? "MAIN BREAK" : "SECOND BREAK", 48, 6, infoW, 14, Justification::centredLeft);
    g.setFont (serif (21.0f));
    g.setColour (ink.withAlpha (k != nullptr ? 1.0f : 0.5f));
    g.drawFittedText (k != nullptr && k->audio != nullptr ? k->audio->name : String ("click to load"),
                      48, getHeight() - 30, getWidth() - 104, 26, Justification::centredLeft, 1, 0.7f);
}

//==============================================================================
JSliceEditor::JSliceEditor (JSliceProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      syncBtn (p.apvts, PID::syncOn, "Sync"),
      cardA (p, 0), cardB (p, 1), waveA (p, 0), waveB (p, 1),
      grid (p), inspector (p),
      duoBox (p.apvts, PID::duoMode, ""),
      autoFillBtn (p.apvts, PID::fillOn, "Auto fill"),
      fillEvery (p.apvts, PID::fillEvery, "")
{
    using namespace juce;
    setSkin (proc.settings().getIntValue ("skin", 0));
    lnf.applySkin();
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    // ---------------------------------------------------------------- header
    swingBox.attachTo (*s.getParameter (PID::swing), [] (double v) { return pct (v); });
    swingBox.setTooltip ("Shuffle of the off-beat 16ths");
    pitchBox.attachTo (*s.getParameter (PID::pitch), [] (double v) { return (v > 0.005 ? "+" : "") + String (v, 1); });
    pitchBox.pixelsForFullRange = 480.0;
    pitchBox.setTooltip ("Global pitch in semitones (double-click to type)");
    for (auto* b : { &swingBox, &pitchBox }) addAndMakeVisible (b);

    for (auto* b : { &browseBtn, &playBtn, &skinBtn }) addAndMakeVisible (b);
    browseBtn.setClickingTogglesState (true);
    browseBtn.setTooltip ("Sample browser : favourite folders, load on click, arrow keys to audition breaks.\nLoads into the selected break (A or B).");
    browseBtn.onClick = [this]
    {
        if (browseBtn.getToggleState())
        {
            browser = std::make_unique<BrowserPanel> (proc, [this] (const File& f) { loadFile (activeSlot, f); },
                                                      [this] { browseBtn.setToggleState (false, sendNotification); });
            addAndMakeVisible (*browser);
            browser->showFile (proc.getCurrentFile (activeSlot));
            resized();
        }
        else browser.reset();
    };
    playBtn.setTooltip ("Play / stop on its own (the DAW transport also starts it)");
    playBtn.onClick = [this] { proc.internalPlay.store (! proc.internalPlay.load()); };
    skinBtn.onClick = [this] { applySkin ((skinIndex() + 1) % numSkins()); };
    skinBtn.setTooltip ("Change the colour skin");

    syncBtn.setTooltip ("SYNC on : follows the DAW tempo and bar position, the breaks are fitted to the project tempo.\n"
                        "SYNC off : runs at its own BPM (set it in the BPM box).");
    addAndMakeVisible (syncBtn);
    bpmBox.get = [this] { return proc.getLastBpm(); };
    bpmBox.set = [this] (double v)
    {
        if (auto* fp = proc.apvts.getParameter (PID::freeBpm)) fp->setValueNotifyingHost (fp->convertTo0to1 ((float) v));
        if (auto* sp = proc.apvts.getParameter (PID::syncOn)) if (sp->getValue() > 0.5f) sp->setValueNotifyingHost (0.0f);
    };
    bpmBox.format = [] (double v) { return String (v, 1); };
    bpmBox.minValue = 60.0; bpmBox.maxValue = 220.0; bpmBox.interval = 0.1; bpmBox.pixelsForFullRange = 600.0;
    bpmBox.showBar = false;
    bpmBox.setTooltip ("Tempo in use. With SYNC on it shows the DAW tempo ; dragging it switches to FREE tempo.");
    addAndMakeVisible (bpmBox);

    // ---------------------------------------------------------------- breaks
    for (auto* c : { &cardA, &cardB })
    {
        addAndMakeVisible (c);
        const int slot = c->slot;
        c->onClick = [this, slot] { setActiveSlot (slot); if (proc.getKit (slot) == nullptr) openFileDialog (slot); };
        c->onSibling = [this, slot] (int d) { auto f = proc.getSiblingFile (slot, d); if (f.existsAsFile()) loadFile (slot, f); };
    }
    for (auto* w : { &waveA, &waveB })
    {
        addAndMakeVisible (w);
        const int slot = w == &waveA ? 0 : 1;
        w->onActivate = [this, slot] { setActiveSlot (slot); };
        w->onEmptyClick = [this, slot] { openFileDialog (slot); };
    }

    sliceMode.addItemList ({ "Smart", "Transient", "Grid" }, 1);
    sliceMode.setTooltip ("Smart: the musically important hits (grid aware, flams merged, ghosts kept when strong)\n"
                          "Transient: every transient above the sensitivity\nGrid: equal divisions snapped to nearby hits");
    gridDiv.addItem ("1/4", 4); gridDiv.addItem ("1/8", 8); gridDiv.addItem ("1/16", 16); gridDiv.addItem ("1/32", 32);
    gridDiv.setTooltip ("Number of slices per bar in Grid mode");
    sliceMode.onChange = [this] { pushSlicer(); };
    gridDiv.onChange = [this] { pushSlicer(); };
    addAndMakeVisible (sliceMode);
    addAndMakeVisible (gridDiv);

    sensBox.get = [this] { return (double) proc.getSlicerSettings (activeSlot).sensitivity; };
    sensBox.set = [this] (double v) { auto st = proc.getSlicerSettings (activeSlot); st.sensitivity = (float) v; proc.setSlicerSettings (activeSlot, st); };
    sensBox.format = [] (double v) { return pct (v); };
    sensBox.interval = 0.01;
    sensBox.setTooltip ("More = more slices (ghost notes, hats). Less = only the main hits. Updates live while you drag.");
    prerollBox.get = [this] { return (double) proc.getSlicerSettings (activeSlot).prerollMs; };
    prerollBox.set = [this] (double v) { auto st = proc.getSlicerSettings (activeSlot); st.prerollMs = (float) v; proc.setSlicerSettings (activeSlot, st); };
    prerollBox.format = [] (double v) { return String (v, 1) + " ms"; };
    prerollBox.maxValue = 10.0; prerollBox.interval = 0.1;
    prerollBox.setTooltip ("Cut this much before each transient so the attack keeps its punch");
    breakBpmBox.get = [this] { auto k = proc.getKit (activeSlot); return k != nullptr ? k->bpm : 0.0; };
    breakBpmBox.set = [this] (double v) { setBreakBpm (v); };
    breakBpmBox.format = [this] (double v) { return proc.getKit (activeSlot) != nullptr ? String (v, 2) : String ("--"); };
    breakBpmBox.minValue = 50.0; breakBpmBox.maxValue = 220.0; breakBpmBox.interval = 0.01; breakBpmBox.pixelsForFullRange = 1500.0;
    breakBpmBox.showBar = false;
    breakBpmBox.setTooltip ("Original tempo of the selected break (detected). Drag, or double-click to type the exact value.");
    breakBarsBox.get = [this] { auto k = proc.getKit (activeSlot); return k != nullptr ? (double) k->bars : 0.0; };
    breakBarsBox.set = [this] (double v) { setBreakBars ((int) v); };
    breakBarsBox.format = [] (double v) { return String ((int) v); };
    breakBarsBox.minValue = 1.0; breakBarsBox.maxValue = 16.0; breakBarsBox.interval = 1.0; breakBarsBox.pixelsForFullRange = 300.0;
    breakBarsBox.showBar = false;
    breakBarsBox.setTooltip ("Length of the selected break in bars (the tempo follows)");
    for (auto* b : { &sensBox, &prerollBox, &breakBpmBox, &breakBarsBox }) addAndMakeVisible (b);

    for (auto* b : { &halfBtn, &dblBtn, &detectBtn }) addAndMakeVisible (b);
    halfBtn.setTooltip ("Halve the break tempo");
    dblBtn.setTooltip ("Double the break tempo");
    detectBtn.setTooltip ("Back to the automatically detected tempo");
    halfBtn.onClick = [this] { if (auto k = proc.getKit (activeSlot)) proc.setBreakTempo (activeSlot, k->bpm * 0.5, jmax (1, k->bars / 2), true); };
    dblBtn.onClick  = [this] { if (auto k = proc.getKit (activeSlot)) proc.setBreakTempo (activeSlot, k->bpm * 2.0, k->bars * 2, true); };
    detectBtn.onClick = [this] { proc.redetectTempo (activeSlot); };

    keepPattern.setToggleState (proc.keepPatternOnLoad(), dontSendNotification);
    keepPattern.onClick = [this] { proc.setKeepPatternOnLoad (keepPattern.getToggleState()); };
    keepPattern.setTooltip ("When loading another break, keep the groove and map each hit to the same kind of drum");
    addAndMakeVisible (keepPattern);

    // ---------------------------------------------------------------- pattern slots
    addAndMakeVisible (patternBar);
    patternBar.setTooltip ("Patterns: click a slot to edit it (while playing, it plays when the current pattern ends). "
                           "Each slot keeps its own pattern, length and generator settings.\n"
                           "CHAIN plays the list in a loop: + adds the edited slot, click a chip for more repeats, right-click removes it.");

    // ---------------------------------------------------------------- lanes
    addAndMakeVisible (grid);
    addAndMakeVisible (inspector);
    grid.onSelect = [this] (int idx) { inspector.setIndex (idx); };
    grid.onViewChanged = [this] { refreshZoom(); };
    grid.onNeedBreak = [this] (int slot) { setActiveSlot (slot); openFileDialog (slot); };

    for (auto* b : { &pagePrev, &pageNext, &fillViewBtn, &undoBtn, &redoBtn }) addAndMakeVisible (b);
    pagePrev.setTooltip ("Previous bars");
    pageNext.setTooltip ("Next bars (after the last bar : the FILL bar)");
    pagePrev.onClick = [this] { grid.page (-1); };
    pageNext.onClick = [this] { grid.page (1); };
    fillViewBtn.setTooltip ("Show the FILL bar (it replaces the last bar every 2 / 4 / 8 bars when AUTO FILL is on, or once with FILL!)");
    fillViewBtn.onClick = [this] { grid.showFill (! grid.isShowingFill()); };
    for (int z : { 1, 2, 4 })
    {
        auto* b = zoomBtns.add (new TextButton (String (z)));
        b->setTooltip ("Show " + String (z) + (z > 1 ? " bars" : " bar") + " at once");
        b->onClick = [this, z] { grid.setZoom (z); };
        addAndMakeVisible (b);
    }
    undoBtn.setTooltip ("Undo (MIDI: D#0)");
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };

    duoBox.box.setTooltip ("How break B shares the steps with break A (never two hits on the same step) :\n"
                           "Fill the gaps : B plays where A rests\nCall / response : A on the first half of each bar, B answers\n"
                           "Split roles : A keeps the kicks and snares, B plays the hats and ghosts");
    addAndMakeVisible (duoBox);
    shareBox.attachTo (*s.getParameter (PID::bShare), [] (double v) { return pct (v); });
    shareBox.setTooltip ("How much of the groove break B takes");
    shareBox.showBar = false;
    addAndMakeVisible (shareBox);
    stepsBox.attachTo (*s.getParameter (PID::steps), [] (double v) { return String ((int) v); });
    stepsBox.pixelsForFullRange = 500.0;
    stepsBox.showBar = false;
    stepsBox.setTooltip ("Pattern length in steps (16 = 1 bar, up to 128 = 8 bars). Drag, wheel, or double-click to type.");
    addAndMakeVisible (stepsBox);

    // ---------------------------------------------------------------- generator
    styleBox.addItemList (bc::getStyleNames(), 1);
    styleBox.setTooltip ("Loads the style's settings and generates a new pattern");
    styleBox.onChange = [this] { proc.applyStyle (styleBox.getSelectedItemIndex()); };
    addAndMakeVisible (styleBox);
    for (auto* b : { &generateBtn, &mutateBtn, &fillBtn }) addAndMakeVisible (b);
    generateBtn.setTooltip ("New pattern (locked steps are kept). MIDI: C0 or the 'Trigger: generate' parameter");
    mutateBtn.setTooltip ("Small variations of the current pattern. MIDI: C#0");
    fillBtn.setTooltip ("Play the fill bar once, at the next bar. MIDI: D0");
    fillBtn.getProperties().set ("hot", true);
    generateBtn.onClick = [this] { proc.generate(); };
    mutateBtn.onClick = [this] { proc.mutate(); };
    fillBtn.onClick = [this] { proc.requestFill(); };
    autoFillBtn.setTooltip ("Play the FILL bar automatically (it replaces the last bar of every 2 / 4 / 8 bars)");
    addAndMakeVisible (autoFillBtn);
    fillEvery.box.setTooltip ("How often the fill bar plays when AUTO FILL is on");
    addAndMakeVisible (fillEvery);

    complexityFader = std::make_unique<ZoneFader> (s, PID::complexity, "Complexity", StringArray { "Clean", "Light", "Choppy", "Wild", "Mad" });
    energyFader = std::make_unique<ZoneFader> (s, PID::energy, "Energy", StringArray { "Sparse", "Light", "Rolling", "Busy", "Full" });
    chaosFader = std::make_unique<ZoneFader> (s, PID::chaos, "Chaos", StringArray { "Tight", "Loose", "Spicy", "Crazy", "Broken" });
    complexityFader->setTooltip ("Cutting : chops, 16th cuts, rolls, repeats, fills.\nLeft = none, centre = the style as is, right = maximum. Heard live : the base pattern stays the same.");
    energyFader->setTooltip ("Hits : density, ghost notes, hats.\nLeft = one or two hits per bar, centre = the style as is, right = maximum. Heard live.");
    chaosFader->setTooltip ("Surprises : stutters, reversed hits, pitch jumps, short hits.\nLeft = none, centre = the style as is, right = maximum. Heard live.");
    for (auto* f : { complexityFader.get(), energyFader.get(), chaosFader.get() }) addAndMakeVisible (f);

    // ---------------------------------------------------------------- tabs
    for (auto* b : { &tabGen, &tabPlay, &tabColour, &tabSpace })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        addAndMakeVisible (b);
    }
    tabGen.onClick = [this] { showTab (0); };
    tabPlay.onClick = [this] { showTab (1); };
    tabColour.onClick = [this] { showTab (2); };
    tabSpace.onClick = [this] { showTab (3); };
    addAndMakeVisible (pageGen);
    addChildComponent (pagePlay);
    addChildComponent (pageColour);
    addChildComponent (pageSpace);

    // DETAILS
    const std::pair<const char*, const char*> genKnobs[] = {
        { PID::density, "Density" }, { PID::backbone, "Backbone" }, { PID::ghosts, "Ghosts" }, { PID::hats, "Hats" },
        { PID::chop, "Chop" }, { PID::cut16, "16th cut" }, { PID::maxRepeats, "Repeats" }, { PID::rolls, "Rolls" },
        { PID::stutter, "Stutter" }, { PID::reverse, "Reverse" }, { PID::pitchProb, "Pitch %" }, { PID::pitchRange, "Pitch rng" },
        { PID::gateProb, "Short" }, { PID::variation, "Variation" }, { PID::fillAmt, "Fill amt" }, { PID::humanize, "Humanize" } };
    for (auto& [id, cap] : genKnobs)
    {
        auto& k = knob (pageGen, id, cap);
        if (String (id) != PID::maxRepeats && String (id) != PID::pitchRange)
            k.format = [] (double v) { return String (roundToInt (v * 100.0)); };
        else
            k.format = [] (double v) { return String (roundToInt (v)); };
    }
    seedEdit.setFont (serif (20.0f));
    seedEdit.setInputRestrictions (9, "0123456789");
    seedEdit.setJustification (Justification::centred);
    seedEdit.setTooltip ("Same seed + same settings = same pattern. Type a number and press Enter.");
    seedEdit.onReturnKey = [this] { proc.generate ((uint32_t) seedEdit.getText().getLargeIntValue()); unfocusAllComponents(); };
    pageGen.addAndMakeVisible (seedEdit);
    clearLocksBtn.setTooltip ("Unlock every step");
    clearLocksBtn.onClick = [this] { proc.clearLocks(); };
    clearFxBtn.setTooltip ("Remove the per-step effects from every step");
    clearFxBtn.onClick = [this] { proc.clearStepFx ((uint8_t) 0x0f); };
    pageGen.addAndMakeVisible (clearLocksBtn);
    pageGen.addAndMakeVisible (clearFxBtn);

    // PLAYBACK
    choice (pagePlay, PID::tempoMode, "Tempo mode");
    knob (pagePlay, PID::cycleMs, "Grain").format = [] (double v) { return String (roundToInt (v)); };
    choice (pagePlay, PID::voiceMode, "Voices");
    choice (pagePlay, PID::tailMode, "Slice tail");
    knob (pagePlay, PID::attack, "Attack").format = [] (double v) { return String (v, 1); };
    knob (pagePlay, PID::decay, "Tight").format = [] (double v) { return String (roundToInt (v * 100.0)); };
    knob (pagePlay, PID::velSens, "Velocity").format = [] (double v) { return String (roundToInt (v * 100.0)); };
    toggle (pagePlay, PID::seqOn, "Sequencer").setTooltip ("Sequencer on / off. Off = play the slices from MIDI notes (C1 = slice 1 of A, C4 = slice 1 of B)");
    toggle (pagePlay, PID::quantize, "Next bar").setTooltip ("On : a new or edited pattern starts at the next bar (no glitch mid-bar)\nOff : changes are heard immediately");

    // COLOUR
    auto p100 = [] (double v) { return String (roundToInt (v * 100.0)); };
    toggle (pageColour, PID::crushOn, "Vintage");
    knob (pageColour, PID::bits, "Bits").format = [] (double v) { return String (v, 1); };
    knob (pageColour, PID::rate, "Rate").format = [] (double v) { return String (v / 1000.0, 1) + "k"; };
    toggle (pageColour, PID::filterOn, "Filter");
    choice (pageColour, PID::filterType, "");
    knob (pageColour, PID::cutoff, "Cutoff").format = [] (double v) { return v >= 1000.0 ? String (v / 1000.0, 1) + "k" : String (roundToInt (v)); };
    knob (pageColour, PID::reso, "Reso").format = p100;
    toggle (pageColour, PID::driveOn, "Drive");
    knob (pageColour, PID::drive, "Drive").format = p100;
    toggle (pageColour, PID::compOn, "Squash");
    knob (pageColour, PID::compAmt, "Amount").format = p100;
    knob (pageColour, PID::compAtk, "Attack").format = [] (double v) { return String (v, 1); };
    knob (pageColour, PID::compRel, "Release").format = [] (double v) { return String (roundToInt (v)); };
    knob (pageColour, PID::compMix, "Mix").format = p100;

    // SPACE
    toggle (pageSpace, PID::dlyOn, "Delay");
    choice (pageSpace, PID::dlyTime, "Time");
    choice (pageSpace, PID::dlySrc, "Source");
    toggle (pageSpace, PID::dlyPing, "Ping-pong");
    knob (pageSpace, PID::dlyFb, "Feedback").format = p100;
    knob (pageSpace, PID::dlyTone, "Tone").format = p100;
    knob (pageSpace, PID::dlyMix, "Level").format = p100;
    toggle (pageSpace, PID::revOn, "Reverb");
    choice (pageSpace, PID::revSrc, "Source");
    knob (pageSpace, PID::revSize, "Size").format = p100;
    knob (pageSpace, PID::revDamp, "Damp").format = p100;
    knob (pageSpace, PID::revWidth, "Width").format = p100;
    knob (pageSpace, PID::revMix, "Level").format = p100;
    knob (pageSpace, PID::output, "Output").format = [] (double v) { return (v > 0.05 ? "+" : "") + String (v, 1); };

    tabGen.setToggleState (true, dontSendNotification);
    showTab (0);
    applySkin (skinIndex());

    proc.addChangeListener (this);
    setSize (kW, kH);
    setActiveSlot (0);
    refreshZoom();
    refreshAll();
    startTimerHz (30);
    proc.updates().check (false);
}

JSliceEditor::~JSliceEditor()
{
    proc.removeChangeListener (this);
    stopTimer();
    setLookAndFeel (nullptr);
}

ui::Knob& JSliceEditor::knob (juce::Component& page, const char* id, const juce::String& caption)
{
    auto k = std::make_unique<Knob> (proc.apvts, id, caption);
    auto* raw = k.get();
    page.addAndMakeVisible (raw);
    byId[id] = raw;
    owned.push_back (std::move (k));
    return *raw;
}

ui::ParamChoice& JSliceEditor::choice (juce::Component& page, const char* id, const juce::String& caption)
{
    auto c = std::make_unique<ParamChoice> (proc.apvts, id, caption);
    auto* raw = c.get();
    page.addAndMakeVisible (raw);
    byId[id] = raw;
    owned.push_back (std::move (c));
    return *raw;
}

ui::ParamToggle& JSliceEditor::toggle (juce::Component& page, const char* id, const juce::String& text)
{
    auto t = std::make_unique<ParamToggle> (proc.apvts, id, text);
    auto* raw = t.get();
    page.addAndMakeVisible (raw);
    byId[id] = raw;
    owned.push_back (std::move (t));
    return *raw;
}

//==============================================================================
void JSliceEditor::applySkin (int index)
{
    setSkin (index);
    proc.settings().setValue ("skin", skinIndex());
    proc.settings().saveIfNeeded();
    lnf.applySkin();
    generateBtn.setColour (juce::TextButton::buttonColourId, pal().accent);
    generateBtn.setColour (juce::TextButton::textColourOffId, pal().onAccent);
    skinBtn.setButtonText ("Skin " + pal().name);
    sendLookAndFeelChange();
    inspector.refresh();
    repaint();
}

void JSliceEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto& pl = pal();
    g.fillAll (pl.bg);

    // logo
    g.setFont (serifItalic (52.0f));
    g.setColour (pl.text);
    g.drawText ("Kinetik", 16, 2, 170, 56, Justification::centredLeft);
    drawStar (g, { 176.0f, 16.0f }, 7.0f, pl.hot);
    g.setFont (mono (13.0f, true, 0.22f));
    g.setColour (pl.text);
    g.drawText ("KJNG-2", 196, 14, 130, 18, Justification::centredLeft);
    g.setFont (mono (9.5f, true, 0.02f));
    if (proc.updates().getState() == upd::Checker::available)
    {
        g.setColour (pl.hot);
        g.drawText ("UPDATE " + String (proc.updates().manifest().version) + " - CLICK", 196, 34, 200, 14, Justification::centredLeft);
    }
    else
    {
        g.setColour (pl.dim);
        g.drawText ("DUAL BREAK JUNGLE", 196, 34, 130, 14, Justification::centredLeft);
    }

    drawPanel (g, { 8, 66, kW - 16, 212 });
    drawPanel (g, { 8, 286, kW - 16, 72 });
    drawPanel (g, { 8, 366, 820, 322 });
    drawPanel (g, { 836, 366, kW - 844, 322 });
    drawPanel (g, { 8, 696, 462, 238 });
    drawPanel (g, { 478, 696, kW - 486, 238 });

    // slicer row : which break it edits
    {
        const auto col = breakColour (activeSlot);
        g.setFont (mono (11.0f));
        g.setColour (pl.dim);
        g.drawText ("SLICER", 16, 216, 70, 14, Justification::centredLeft);
        g.setFont (serif (34.0f));
        g.setColour (col);
        g.drawText (activeSlot == 0 ? "A" : "B", 16, 228, 40, 38, Justification::centredLeft);
        g.setFont (mono (11.0f));
        g.setColour (pl.dim);
        g.drawText ("MODE", 92, 214, 80, 14, Justification::centredLeft);
        g.drawText ("GRID", 336, 214, 60, 14, Justification::centredLeft);
    }

    // lanes toolbar
    {
        g.setFont (serif (27.0f));
        g.setColour (pl.text);
        const auto title = grid.pageTitle();
        g.drawText (title, 50, 372, 112, 34, Justification::centredLeft);
        const int tw = GlyphArrangement::getStringWidthInt (serif (27.0f), title);
        g.setFont (serif (19.0f));
        g.setColour (pl.dim);
        g.drawText (grid.pageSuffix(), 54 + tw, 376, 60, 30, Justification::centredLeft);
        g.setFont (mono (11.0f));
        g.drawText ("DUO", 352, 374, 34, 30, Justification::centredRight);
    }

    // generator captions
    g.setFont (mono (11.0f));
    g.setColour (pl.dim);
    g.drawText ("STYLE", 16, 704, 120, 14, Justification::centredLeft);
    g.setFont (mono (10.0f, true, 0.04f));
    g.drawFittedText ("MIDI : C0 GENERATE  C#0 MUTATE  D0 FILL\nC1 = SLICES OF A   C4 = SLICES OF B", 16, 886, 200, 36, Justification::centredLeft, 2);

    if (errorText.isNotEmpty())
    {
        g.setColour (pl.hot);
        g.setFont (mono (11.5f));
        g.drawFittedText (errorText, 480, 12, 214, 44, Justification::centredLeft, 3);
    }
}

void JSliceEditor::resized()
{
    using namespace juce;
    // ---- header
    swingBox.setBounds (330, 12, 66, 44);
    pitchBox.setBounds (402, 12, 66, 44);
    browseBtn.setBounds (700, 12, 96, 44);
    playBtn.setBounds (802, 12, 96, 44);
    syncBtn.setBounds (904, 12, 56, 44);
    bpmBox.setBounds (966, 12, 92, 44);
    skinBtn.setBounds (1064, 12, 100, 44);

    // ---- breaks
    cardA.setBounds (16, 74, 230, 64);
    cardB.setBounds (16, 144, 230, 64);
    waveA.setBounds (252, 74, kW - 268, 64);
    waveB.setBounds (252, 144, kW - 268, 64);
    sliceMode.setBounds (92, 232, 116, 32);
    sensBox.setBounds (214, 216, 116, 52);
    gridDiv.setBounds (336, 232, 76, 32);
    prerollBox.setBounds (418, 216, 100, 52);
    keepPattern.setBounds (530, 238, 214, 24);
    breakBpmBox.setBounds (760, 216, 116, 52);
    breakBarsBox.setBounds (882, 216, 70, 52);
    halfBtn.setBounds (960, 234, 44, 30);
    dblBtn.setBounds (1008, 234, 44, 30);
    detectBtn.setBounds (1058, 234, 106, 30);

    // ---- pattern slots
    patternBar.setBounds (16, 293, kW - 32, 58);

    // ---- lanes toolbar
    pagePrev.setBounds (16, 374, 28, 30);
    pageNext.setBounds (162, 374, 28, 30);
    for (int i = 0; i < zoomBtns.size(); ++i) zoomBtns[i]->setBounds (198 + i * 32, 374, 30, 30);
    fillViewBtn.setBounds (298, 374, 50, 30);
    duoBox.setBounds (390, 375, 140, 28);
    shareBox.setBounds (536, 372, 68, 34);
    stepsBox.setBounds (610, 372, 64, 34);
    undoBtn.setBounds (680, 374, 68, 30);
    redoBtn.setBounds (752, 374, 68, 30);
    grid.setBounds (16, 412, 804, 268);
    inspector.setBounds (848, 374, kW - 868, 306);

    // ---- generator
    styleBox.setBounds (16, 720, 194, 30);
    generateBtn.setBounds (16, 756, 194, 48);
    mutateBtn.setBounds (16, 810, 95, 30);
    fillBtn.setBounds (115, 810, 95, 30);
    autoFillBtn.setBounds (16, 846, 95, 28);
    fillEvery.setBounds (115, 846, 95, 28);
    complexityFader->setBounds (222, 706, 240, 68);
    energyFader->setBounds (222, 780, 240, 68);
    chaosFader->setBounds (222, 854, 240, 68);

    // ---- tabs
    tabGen.setBounds (490, 704, 96, 26);
    tabPlay.setBounds (590, 704, 96, 26);
    tabColour.setBounds (690, 704, 96, 26);
    tabSpace.setBounds (790, 704, 96, 26);
    const Rectangle<int> pageR (490, 736, kW - 510, 190);
    for (auto* p : { &pageGen, &pagePlay, &pageColour, &pageSpace }) p->setBounds (pageR);

    auto place = [this] (const char* id, Rectangle<int> r) { if (auto* c = byId[id]) c->setBounds (r); };
    const int W = pageR.getWidth(), H = pageR.getHeight();
    auto knobRow = [&] (std::initializer_list<const char*> ids, int x, int y, int w, int h)
    {
        for (auto* id : ids) { place (id, { x, y, w, h }); x += w; }
    };

    // DETAILS
    {
        const int kw = (W - 78) / 8;
        pageGen.groups = { { { 0, 0, W - 76, H }, "" }, { { W - 70, 0, 70, H }, "Seed" } };
        knobRow ({ PID::density, PID::backbone, PID::ghosts, PID::hats, PID::chop, PID::cut16, PID::maxRepeats, PID::rolls }, 2, 4, kw, 90);
        knobRow ({ PID::stutter, PID::reverse, PID::pitchProb, PID::pitchRange, PID::gateProb, PID::variation, PID::fillAmt, PID::humanize }, 2, 96, kw, 90);
        seedEdit.setBounds (W - 64, 24, 58, 30);
        clearLocksBtn.setBounds (W - 64, 104, 58, 30);
        clearFxBtn.setBounds (W - 64, 140, 58, 30);
    }
    // PLAYBACK
    {
        pagePlay.groups = { { { 0, 0, 190, H }, "Tempo" }, { { 198, 0, 330, H }, "Voice" }, { { 536, 0, W - 536, H }, "Pattern" } };
        place (PID::tempoMode, { 8, 22, 174, 46 });
        place (PID::cycleMs, { 50, 80, 90, 100 });
        place (PID::voiceMode, { 206, 22, 154, 46 });
        place (PID::tailMode, { 366, 22, 154, 46 });
        knobRow ({ PID::attack, PID::decay, PID::velSens }, 206, 80, 104, 100);
        place (PID::seqOn, { 544, 28, W - 552, 28 });
        place (PID::quantize, { 544, 64, W - 552, 28 });
    }
    // COLOUR
    {
        pageColour.groups = { { { 0, 0, 150, H }, "" }, { { 156, 0, 160, H }, "" }, { { 322, 0, 84, H }, "" }, { { 412, 0, W - 412, H }, "" } };
        place (PID::crushOn, { 8, 8, 134, 26 });
        knobRow ({ PID::bits, PID::rate }, 6, 52, 69, 110);
        place (PID::filterOn, { 164, 8, 70, 26 });
        place (PID::filterType, { 238, 8, 72, 26 });
        knobRow ({ PID::cutoff, PID::reso }, 160, 52, 76, 110);
        place (PID::driveOn, { 330, 8, 68, 26 });
        knobRow ({ PID::drive }, 326, 52, 76, 110);
        place (PID::compOn, { 420, 8, 100, 26 });
        knobRow ({ PID::compAmt, PID::compAtk, PID::compRel, PID::compMix }, 416, 52, (W - 420) / 4, 110);
    }
    // SPACE
    {
        pageSpace.groups = { { { 0, 0, 300, H }, "" }, { { 306, 0, 300, H }, "" }, { { 612, 0, W - 612, H }, "" } };
        place (PID::dlyOn, { 8, 8, 84, 26 });
        place (PID::dlyPing, { 98, 8, 104, 26 });
        place (PID::dlyTime, { 8, 40, 140, 46 });
        place (PID::dlySrc, { 152, 40, 140, 46 });
        knobRow ({ PID::dlyFb, PID::dlyTone, PID::dlyMix }, 6, 92, 96, 94);
        place (PID::revOn, { 314, 8, 84, 26 });
        place (PID::revSrc, { 314, 40, 140, 46 });
        knobRow ({ PID::revSize, PID::revDamp, PID::revWidth, PID::revMix }, 310, 92, 73, 94);
        knobRow ({ PID::output }, 614, 92, W - 616, 94);
    }

    if (browser != nullptr)
        browser->setBounds (330, 62, 440, kH - 74);
}

void JSliceEditor::showTab (int t)
{
    pageGen.setVisible (t == 0);
    pagePlay.setVisible (t == 1);
    pageColour.setVisible (t == 2);
    pageSpace.setVisible (t == 3);
}

//==============================================================================
void JSliceEditor::setActiveSlot (int slot)
{
    activeSlot = juce::jlimit (0, 1, slot);
    cardA.active = activeSlot == 0;
    cardB.active = activeSlot == 1;
    cardA.repaint();
    cardB.repaint();
    if (browser != nullptr) browser->showFile (proc.getCurrentFile (activeSlot));
    refreshAll();
    repaint (0, 210, 92, 64);
}

void JSliceEditor::refreshZoom()
{
    for (int i = 0; i < zoomBtns.size(); ++i)
        zoomBtns[i]->setToggleState (! grid.isShowingFill() && grid.getZoom() == (1 << (i == 2 ? 2 : i)), juce::dontSendNotification);
    fillViewBtn.setToggleState (grid.isShowingFill(), juce::dontSendNotification);
    repaint (16, 370, 340, 38);
}

void JSliceEditor::refreshAll()
{
    using namespace juce;
    waveA.refresh();
    waveB.refresh();
    grid.refresh();
    inspector.refresh();
    cardA.repaint();
    cardB.repaint();

    const bool hasKit = proc.getKit (activeSlot) != nullptr;
    const auto& st = proc.getSlicerSettings (activeSlot);
    sliceMode.setSelectedId (st.mode + 1, dontSendNotification);
    gridDiv.setSelectedId (st.gridDivision, dontSendNotification);
    gridDiv.setEnabled (st.mode == bc::SlicerSettings::Grid);
    for (auto* c : std::initializer_list<Component*> { &sliceMode, &sensBox, &prerollBox, &breakBpmBox, &breakBarsBox, &halfBtn, &dblBtn, &detectBtn })
        c->setEnabled (hasKit);
    for (auto* b : { &sensBox, &prerollBox, &breakBpmBox, &breakBarsBox }) b->repaint();

    if (! seedEdit.hasKeyboardFocus (true)) seedEdit.setText (String ((int) proc.getPattern().seed), false);
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
    patternBar.repaint();
}

void JSliceEditor::pushSlicer()
{
    auto st = proc.getSlicerSettings (activeSlot);
    st.mode = sliceMode.getSelectedId() - 1;
    st.gridDivision = gridDiv.getSelectedId() > 0 ? gridDiv.getSelectedId() : 16;
    gridDiv.setEnabled (st.mode == bc::SlicerSettings::Grid);
    proc.setSlicerSettings (activeSlot, st);
}

void JSliceEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll();
}

void JSliceEditor::timerCallback()
{
    grid.tick();
    waveA.tick();
    waveB.tick();
    for (auto* b : { &swingBox, &pitchBox, &bpmBox, &stepsBox, &shareBox, &sensBox, &prerollBox, &breakBpmBox, &breakBarsBox }) b->refresh();

    const int style = (int) proc.apvts.getRawParameterValue (PID::style)->load();
    if (styleBox.getSelectedItemIndex() != style) styleBox.setSelectedItemIndex (style, juce::dontSendNotification);

    const bool running = proc.isRunning();
    const bool sync = proc.apvts.getRawParameterValue (PID::syncOn)->load() > 0.5f;
    const bool hostDriven = sync && proc.hostPlaying.load();
    playBtn.setToggleState (running, juce::dontSendNotification);
    playBtn.setButtonText (running ? (hostDriven ? utf8 ("\xe2\x96\xa0 Host") : utf8 ("\xe2\x96\xa0 Stop")) : utf8 ("\xe2\x96\xb6 Play"));
    const auto bpmLabel = juce::String (sync && proc.getHostBpm() > 1.0 ? utf8 ("BPM \xc2\xb7 HOST") : utf8 ("BPM \xc2\xb7 FREE"));
    if (bpmBox.label != bpmLabel) { bpmBox.label = bpmLabel; bpmBox.repaint(); }

    const auto ep = proc.currentEngineParams();
    if (ep.patternSteps != lastSteps || ep.fillEvery != lastFill)
    {
        lastSteps = ep.patternSteps; lastFill = ep.fillEvery;
        grid.refresh();
    }
    const bool bActive = proc.isBreakBActive();
    if (bActive != lastBActive)
    {
        lastBActive = bActive;
        waveB.dimmed = ! bActive;
        waveB.repaint();
        cardB.repaint();
        grid.repaint();
    }
    const auto title = grid.pageTitle() + grid.pageSuffix();
    if (title != lastPageTitle) { lastPageTitle = title; repaint (16, 370, 340, 38); }
    patternBar.repaint();

    if (errorTicks > 0 && --errorTicks == 0) { errorText.clear(); repaint (476, 8, 222, 52); }

    const int us = (int) proc.updates().getState();
    if (us != lastUpdState)
    {
        lastUpdState = us;
        repaint (0, 0, 420, 64);
        if (updWaiting && us != upd::Checker::checking) { updWaiting = false; showUpdateDialog(); }
    }
}

//==============================================================================
void JSliceEditor::mouseUp (const juce::MouseEvent& e)
{
    if (e.getPosition().y < 62 && e.getPosition().x < 320) showUpdateDialog();
}

void JSliceEditor::showUpdateDialog()
{
    using namespace juce;
    auto& u = proc.updates();
    const auto cur = upd::Checker::currentVersion();
    SafePointer<JSliceEditor> safe (this);

    if (u.getState() == upd::Checker::available)
    {
        const auto m = u.manifest();
        String msg;
        if (! m.notes.empty()) msg << "New: " << String (m.notes) << "\n\n";
        msg << "You have version " << cur << ". Download the new version from its release page and install it like "
               "the first time (close your DAW first).";
        AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::InfoIcon)
                                    .withTitle ("Kinetik Jungle " + String (m.version) + " is available")
                                    .withMessage (msg)
                                    .withButton ("Open download page").withButton ("Skip this version").withButton ("Later")
                                    .withAssociatedComponent (this),
                                [safe] (int r)
                                {
                                    if (safe == nullptr) return;
                                    if (r == 1) safe->proc.updates().openPage();
                                    else if (r == 2) safe->proc.updates().skipThisVersion();
                                    safe->repaint (0, 0, 420, 64);
                                });
        return;
    }

    String msg = "Version " + cur + ".\n\n";
    switch (u.getState())
    {
        case upd::Checker::checking: msg << "Looking for a new version..."; break;
        case upd::Checker::upToDate: msg << "You have the latest version."; break;
        case upd::Checker::failed:   msg << "Could not reach the update server (or its answer was not genuine). Check your internet connection and try again."; break;
        default:                     msg << "Click Check now to look for a new version."; break;
    }
    msg << "\n\nAutomatic check (once a day, nothing is sent): " << (u.autoCheck() ? "ON" : "OFF");
    AlertWindow::showAsync (MessageBoxOptions().withIconType (MessageBoxIconType::NoIcon)
                                .withTitle ("Kinetik Jungle updates")
                                .withMessage (msg)
                                .withButton ("Check now").withButton (u.autoCheck() ? "Turn automatic check off" : "Turn automatic check on").withButton ("Close")
                                .withAssociatedComponent (this),
                            [safe] (int r)
                            {
                                if (safe == nullptr) return;
                                auto& up = safe->proc.updates();
                                if (r == 1) { up.check (true); safe->updWaiting = true; safe->lastUpdState = -1; }
                                else if (r == 2) { up.setAutoCheck (! up.autoCheck()); safe->showUpdateDialog(); }
                            });
}

//==============================================================================
void JSliceEditor::loadFile (int slot, const juce::File& f)
{
    juce::String err;
    if (! proc.loadFile (slot, f, err)) showError (err);
    else setActiveSlot (slot);
}

void JSliceEditor::openFileDialog (int slot)
{
    using namespace juce;
    File start (proc.settings().getValue ("lastDir"));
    chooser = std::make_unique<FileChooser> (slot == 0 ? "Load break A" : "Load break B", start, JSliceProcessor::getSupportedWildcard());
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this, slot] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (slot, fc.getResult()); });
}

void JSliceEditor::showError (const juce::String& msg)
{
    errorText = msg;
    errorTicks = 30 * 5;
    repaint();
}

void JSliceEditor::setBreakBpm (double bpm)
{
    auto k = proc.getKit (activeSlot);
    if (k == nullptr || bpm < 30.0 || bpm > 400.0) return;
    const int bars = bc::Analyzer::barsForTempo (k->length(), k->audio->sampleRate, bpm, k->gridOrigin);
    proc.setBreakTempo (activeSlot, bpm, bars, true);
}

void JSliceEditor::setBreakBars (int bars)
{
    auto k = proc.getKit (activeSlot);
    if (k == nullptr || bars <= 0 || bars == k->bars) return;
    const double dur = (double) (k->length() - k->gridOrigin) / k->audio->sampleRate;
    proc.setBreakTempo (activeSlot, bars * 240.0 / dur, bars, true);
}

bool JSliceEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;wave;aif;aiff;aifc;flac;mp3;ogg")) return true;
    return false;
}

void JSliceEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::Array<juce::File> audio;
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.hasFileExtension ("wav;wave;aif;aiff;aifc;flac;mp3;ogg")) audio.add (file);
    }
    if (audio.isEmpty()) return;
    if (audio.size() >= 2) { loadFile (0, audio[0]); loadFile (1, audio[1]); return; }

    // dropped on the B row (card, waveform or lane B) -> B, on the A row -> A, elsewhere -> the selected break
    const juce::Point<int> pt (x, y);
    int slot = activeSlot;
    if (cardB.getBounds().contains (pt) || waveB.getBounds().contains (pt)) slot = 1;
    else if (cardA.getBounds().contains (pt) || waveA.getBounds().contains (pt)) slot = 0;
    loadFile (slot, audio[0]);
}

juce::AudioProcessorEditor* JSliceProcessor::createEditor()
{
    return new JSliceEditor (*this);
}
