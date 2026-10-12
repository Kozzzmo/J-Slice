#include "PluginEditor.h"

using namespace ui;

namespace
{
    constexpr int kW = 1180, kH = 910;

    juce::String pct (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; }
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
            g.drawText (gr.title.toUpperCase(), gr.r.getX() + 10, gr.r.getY() + 6, gr.r.getWidth() - 20, 16, juce::Justification::centredLeft);
        }
    }
}

void NameBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (pal().inset);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (isMouseOver() ? pal().dim : pal().line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    auto inner = getLocalBounds().reduced (10, 0);
    g.setFont (mono (10.5f));
    g.setColour (pal().dim);
    g.drawText ("BREAK", inner.removeFromLeft (56), juce::Justification::centredLeft);
    g.setFont (serif (26.0f));
    g.setColour (pal().text);
    g.drawFittedText (name, inner, juce::Justification::centredRight, 1, 0.6f);
}

//==============================================================================
JSliceEditor::JSliceEditor (JSliceProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      syncBtn (p.apvts, PID::syncOn, "Sync"),
      wave (p), grid (p),
      seqBtn (p.apvts, PID::seqOn, "Seq"),
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

    nameBox.setTooltip ("Click to open a file. You can also drop a file anywhere on the plugin.");
    nameBox.onClick = [this] { openFileDialog(); };
    addAndMakeVisible (nameBox);

    for (auto* b : { &prevBtn, &nextBtn, &browseBtn, &playBtn })
        addAndMakeVisible (b);
    prevBtn.setTooltip ("Previous file in the same folder");
    nextBtn.setTooltip ("Next file in the same folder");
    prevBtn.onClick = [this] { auto f = proc.getSiblingFile (-1); if (f.existsAsFile()) loadFile (f); };
    nextBtn.onClick = [this] { auto f = proc.getSiblingFile (1);  if (f.existsAsFile()) loadFile (f); };
    browseBtn.setClickingTogglesState (true);
    browseBtn.setTooltip ("Sample browser : favourite folders, load on click, arrow keys to audition breaks");
    browseBtn.onClick = [this]
    {
        if (browseBtn.getToggleState())
        {
            browser = std::make_unique<BrowserPanel> (proc, [this] (const File& f) { loadFile (f); },
                                                      [this] { browseBtn.setToggleState (false, sendNotification); });
            addAndMakeVisible (*browser);
            browser->showFile (proc.getCurrentFile());
            resized();
        }
        else browser.reset();
    };
    playBtn.setTooltip ("Play / stop J-Slice on its own (the DAW transport also starts it)");
    playBtn.onClick = [this] { proc.internalPlay.store (! proc.internalPlay.load()); };

    syncBtn.setTooltip ("SYNC on : J-Slice follows the DAW tempo and bar position, the break is fitted to the project tempo.\n"
                        "SYNC off : J-Slice runs at its own BPM (set it in the BPM box).");
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
    bpmBox.setTooltip ("Tempo used by J-Slice. With SYNC on it shows the DAW tempo ; dragging it switches to FREE tempo.");
    addAndMakeVisible (bpmBox);

    // ---------------------------------------------------------------- break display + slicer
    addAndMakeVisible (wave);
    sliceMode.addItemList ({ "Smart", "Transient", "Grid" }, 1);
    sliceMode.setTooltip ("Smart: the musically important hits (grid aware, flams merged, ghosts kept when strong)\n"
                          "Transient: every transient above the sensitivity\nGrid: equal divisions snapped to nearby hits");
    gridDiv.addItem ("1/4", 4); gridDiv.addItem ("1/8", 8); gridDiv.addItem ("1/16", 16); gridDiv.addItem ("1/32", 32);
    gridDiv.setTooltip ("Number of slices per bar in Grid mode");
    sliceMode.onChange = [this] { pushSlicer(); };
    gridDiv.onChange = [this] { pushSlicer(); };
    addAndMakeVisible (sliceMode);
    addAndMakeVisible (gridDiv);

    sensBox.get = [this] { return (double) proc.getSlicerSettings().sensitivity; };
    sensBox.set = [this] (double v) { auto st = proc.getSlicerSettings(); st.sensitivity = (float) v; proc.setSlicerSettings (st); };
    sensBox.format = [] (double v) { return pct (v); };
    sensBox.interval = 0.01;
    sensBox.setTooltip ("More = more slices (ghost notes, hats). Less = only the main hits. Updates live while you drag.");
    prerollBox.get = [this] { return (double) proc.getSlicerSettings().prerollMs; };
    prerollBox.set = [this] (double v) { auto st = proc.getSlicerSettings(); st.prerollMs = (float) v; proc.setSlicerSettings (st); };
    prerollBox.format = [] (double v) { return String (v, 1) + " ms"; };
    prerollBox.maxValue = 10.0; prerollBox.interval = 0.1;
    prerollBox.setTooltip ("Cut this much before each transient so the attack keeps its punch");
    breakBpmBox.get = [this] { auto k = proc.getKit(); return k != nullptr ? k->bpm : 0.0; };
    breakBpmBox.set = [this] (double v) { setBreakBpm (v); };
    breakBpmBox.format = [this] (double v) { return proc.getKit() != nullptr ? String (v, 2) : String ("--"); };
    breakBpmBox.minValue = 50.0; breakBpmBox.maxValue = 220.0; breakBpmBox.interval = 0.01; breakBpmBox.pixelsForFullRange = 1500.0;
    breakBpmBox.showBar = false;
    breakBpmBox.setTooltip ("Original tempo of the break (detected). Drag, or double-click to type the exact value.");
    breakBarsBox.get = [this] { auto k = proc.getKit(); return k != nullptr ? (double) k->bars : 0.0; };
    breakBarsBox.set = [this] (double v) { setBreakBars ((int) v); };
    breakBarsBox.format = [] (double v) { return String ((int) v); };
    breakBarsBox.minValue = 1.0; breakBarsBox.maxValue = 16.0; breakBarsBox.interval = 1.0; breakBarsBox.pixelsForFullRange = 300.0;
    breakBarsBox.showBar = false;
    breakBarsBox.setTooltip ("Length of the break in bars (the tempo follows)");
    for (auto* b : { &sensBox, &prerollBox, &breakBpmBox, &breakBarsBox }) addAndMakeVisible (b);

    for (auto* b : { &halfBtn, &dblBtn, &detectBtn }) addAndMakeVisible (b);
    halfBtn.setTooltip ("Halve the break tempo");
    dblBtn.setTooltip ("Double the break tempo");
    detectBtn.setTooltip ("Back to the automatically detected tempo");
    halfBtn.onClick = [this] { if (auto k = proc.getKit()) proc.setBreakTempo (k->bpm * 0.5, jmax (1, k->bars / 2), true); };
    dblBtn.onClick  = [this] { if (auto k = proc.getKit()) proc.setBreakTempo (k->bpm * 2.0, k->bars * 2, true); };
    detectBtn.onClick = [this] { proc.redetectTempo(); };

    keepPattern.setToggleState (proc.keepPatternOnLoad(), dontSendNotification);
    keepPattern.onClick = [this] { proc.setKeepPatternOnLoad (keepPattern.getToggleState()); };
    keepPattern.setTooltip ("When loading another break, keep the groove and map each hit to the same kind of drum");
    addAndMakeVisible (keepPattern);

    // ---------------------------------------------------------------- sequencer
    addAndMakeVisible (grid);
    seqBtn.setTooltip ("Sequencer on / off. Off = play the slices from MIDI notes (C1 = slice 1)");
    addAndMakeVisible (seqBtn);
    stepsBox.attachTo (*s.getParameter (PID::steps), [] (double v) { return "L:" + String ((int) v); });
    stepsBox.pixelsForFullRange = 500.0;
    stepsBox.showBar = false;
    stepsBox.setTooltip ("Pattern length in steps (16 = 1 bar). Drag, use - / +, or double-click to type.");
    addAndMakeVisible (stepsBox);
    auto nudgeSteps = [this] (int d)
    {
        if (auto* sp = proc.apvts.getParameter (PID::steps))
        {
            const int cur = (int) sp->convertFrom0to1 (sp->getValue());
            const int next = jlimit (1, bc::kMaxSteps, cur + d);
            sp->beginChangeGesture();
            sp->setValueNotifyingHost (sp->convertTo0to1 ((float) next));
            sp->endChangeGesture();
        }
    };
    minusBtn.onClick = [nudgeSteps] { nudgeSteps (ModifierKeys::currentModifiers.isShiftDown() ? -16 : -1); };
    plusBtn.onClick  = [nudgeSteps] { nudgeSteps (ModifierKeys::currentModifiers.isShiftDown() ? 16 : 1); };
    minusBtn.setTooltip ("Shorter pattern (shift: one bar)");
    plusBtn.setTooltip ("Longer pattern (shift: one bar)");
    addAndMakeVisible (minusBtn);
    addAndMakeVisible (plusBtn);
    fillEvery.box.setTooltip ("How often the FILL row plays (when FILL is on)");
    addAndMakeVisible (fillEvery);

    for (auto* b : { &undoBtn, &redoBtn, &clearLocksBtn }) addAndMakeVisible (b);
    undoBtn.setTooltip ("Undo (MIDI: D#0)");
    clearLocksBtn.setTooltip ("Unlock every step");
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    clearLocksBtn.onClick = [this] { proc.clearLocks(); };

    seedEdit.setFont (serif (21.0f));
    seedEdit.setInputRestrictions (9, "0123456789");
    seedEdit.setJustification (Justification::centred);
    seedEdit.setTooltip ("Same seed + same settings = same pattern. Type a number and press Enter.");
    seedEdit.onReturnKey = [this] { proc.generate ((uint32_t) seedEdit.getText().getLargeIntValue()); unfocusAllComponents(); };
    addAndMakeVisible (seedEdit);

    // per-step effect edit modes
    {
        const char* names[] = { "Notes", "Delay", "Reverb", "Lo-fi", "Filter" };
        const char* tips[] = {
            "Edit the hits (add, length, re-triggers, reverse...)",
            "Paint delay throws : click or drag across steps. Those hits are sent into the delay (SPACE tab)",
            "Paint reverb throws : click or drag across steps. Those hits are sent into the reverb (SPACE tab)",
            "Paint lo-fi hits : 8 kHz / 5-bit crunch on those hits only",
            "Paint filtered hits : telephone band-pass on those hits only" };
        for (int i = 0; i < 5; ++i)
        {
            auto* b = modeBtns.add (new TextButton (names[i]));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (78);
            b->setTooltip (tips[i]);
            b->onClick = [this, i] { grid.setEditMode (i); };
            addAndMakeVisible (b);
        }
        modeBtns[0]->setToggleState (true, dontSendNotification);
        clearFxBtn.setTooltip ("Remove the effect of the current mode from every step (in Notes mode : every effect)");
        clearFxBtn.onClick = [this]
        {
            const int m = grid.getEditMode();
            proc.clearStepFx (m == 0 ? (uint8_t) 0x0f : (uint8_t) (1 << (m - 1)));
        };
        addAndMakeVisible (clearFxBtn);
    }

    // ---------------------------------------------------------------- generator panel
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

    auto macroFmt = [] (double v) { const int n = roundToInt ((v - 0.5) * 200.0); return (n > 0 ? "+" : "") + String (n); };
    complexityKnob = std::make_unique<Knob> (s, PID::complexity, "Complexity");
    energyKnob = std::make_unique<Knob> (s, PID::energy, "Energy");
    chaosKnob = std::make_unique<Knob> (s, PID::chaos, "Chaos");
    for (auto* k : { complexityKnob.get(), energyKnob.get(), chaosKnob.get() })
    {
        k->format = macroFmt;
        addAndMakeVisible (k);
    }
    complexityKnob->slider.setTooltip ("More cutting : chops, 16th cuts, rolls, repeats, bigger fills (left = none, centre = the style as is, right = maximum)");
    energyKnob->slider.setTooltip ("More hits : density, ghost notes, hats (left = none, centre = the style as is, right = maximum)");
    chaosKnob->slider.setTooltip ("More surprises : stutters, reversed hits, pitch jumps, short hits, variations (left = none, centre = the style as is, right = maximum)");

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
    skinBtn.onClick = [this] { applySkin ((skinIndex() + 1) % numSkins()); };
    skinBtn.setTooltip ("Change the colour skin");
    addAndMakeVisible (skinBtn);

    // GENERATOR
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

    // PLAYBACK
    choice (pagePlay, PID::tempoMode, "Tempo mode");
    knob (pagePlay, PID::cycleMs, "Grain").format = [] (double v) { return String (roundToInt (v)) + "ms"; };
    choice (pagePlay, PID::voiceMode, "Voices");
    choice (pagePlay, PID::tailMode, "Slice tail");
    knob (pagePlay, PID::attack, "Attack").format = [] (double v) { return String (v, 1); };
    knob (pagePlay, PID::decay, "Tight").format = [] (double v) { return String (roundToInt (v * 100.0)); };
    knob (pagePlay, PID::velSens, "Velocity").format = [] (double v) { return String (roundToInt (v * 100.0)); };
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
    refreshAll();
    startTimerHz (30);
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
    skinBtn.setButtonText ("Skin  " + pal().name);
    sendLookAndFeelChange();
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
    g.drawText ("J-Slice", 16, 2, 170, 56, Justification::centredLeft);
    drawStar (g, { 176.0f, 16.0f }, 7.0f, pl.hot);
    g.setFont (mono (13.0f, true, 0.22f));
    g.drawText ("JSLC-16", 196, 14, 130, 18, Justification::centredLeft);
    g.setFont (mono (9.5f, true, 0.02f));
    g.setColour (pl.dim);
    g.drawText ("BREAK SLICER / GENERATOR", 196, 34, 130, 14, Justification::centredLeft);

    drawPanel (g, { 8, 66, kW - 16, 262 });
    drawPanel (g, { 8, 336, 860, 360 });
    drawPanel (g, { 876, 336, 296, 360 });
    drawPanel (g, { 8, 704, kW - 16, 198 });

    // captions of the plain combos
    g.setFont (mono (11.0f));
    g.setColour (pl.dim);
    g.drawText ("SLICER", 16, 267, 120, 15, Justification::centredLeft);
    g.drawText ("GRID", 272, 267, 80, 15, Justification::centredLeft);
    g.drawText ("STYLE", 888, 343, 120, 15, Justification::centredLeft);
    g.drawText ("SEED", 692, 346, 44, 26, Justification::centredRight);

    g.setFont (serifItalic (27.0f));
    g.setColour (pl.text);
    g.drawText ("Sequencer", 18, 344, 120, 30, Justification::centredLeft);

    if (errorText.isNotEmpty())
    {
        g.setColour (pl.hot);
        g.setFont (mono (12.0f));
        g.drawText (errorText, 450, 56, 600, 10, Justification::left);
    }
}

void JSliceEditor::resized()
{
    using namespace juce;
    // ---- header
    swingBox.setBounds (330, 12, 66, 44);
    pitchBox.setBounds (402, 12, 66, 44);
    nameBox.setBounds (476, 12, 286, 44);
    prevBtn.setBounds (768, 12, 28, 44);
    nextBtn.setBounds (800, 12, 28, 44);
    browseBtn.setBounds (836, 12, 80, 44);
    playBtn.setBounds (922, 12, 88, 44);
    syncBtn.setBounds (1016, 12, 56, 44);
    bpmBox.setBounds (1078, 12, 86, 44);

    // ---- break display
    wave.setBounds (16, 74, kW - 32, 186);
    sliceMode.setBounds (16, 284, 120, 32);
    sensBox.setBounds (144, 268, 120, 50);
    gridDiv.setBounds (272, 284, 80, 32);
    prerollBox.setBounds (360, 268, 104, 50);
    keepPattern.setBounds (486, 288, 230, 22);
    breakBpmBox.setBounds (760, 268, 116, 50);
    breakBarsBox.setBounds (882, 268, 70, 50);
    halfBtn.setBounds (960, 284, 44, 30);
    dblBtn.setBounds (1008, 284, 44, 30);
    detectBtn.setBounds (1058, 284, 106, 30);

    // ---- sequencer
    seqBtn.setBounds (136, 346, 52, 26);
    minusBtn.setBounds (198, 346, 26, 26);
    stepsBox.setBounds (228, 340, 74, 34);
    plusBtn.setBounds (306, 346, 26, 26);
    fillEvery.setBounds (340, 346, 140, 26);
    undoBtn.setBounds (488, 346, 58, 26);
    redoBtn.setBounds (550, 346, 58, 26);
    clearLocksBtn.setBounds (612, 346, 76, 26);
    seedEdit.setBounds (740, 346, 120, 26);
    {
        int x = 16;
        for (auto* b : modeBtns) { b->setBounds (x, 380, 88, 26); x += 92; }
        clearFxBtn.setBounds (x + 6, 380, 96, 26);
    }
    grid.setBounds (16, 412, 846, 278);

    // ---- generator panel
    styleBox.setBounds (888, 360, 272, 32);
    generateBtn.setBounds (888, 400, 272, 44);
    mutateBtn.setBounds (888, 450, 132, 30);
    fillBtn.setBounds (1028, 450, 132, 30);
    complexityKnob->setBounds (884, 500, 94, 186);
    energyKnob->setBounds (977, 500, 94, 186);
    chaosKnob->setBounds (1070, 500, 94, 186);

    // ---- tabs
    tabGen.setBounds (16, 712, 104, 26);
    tabPlay.setBounds (124, 712, 96, 26);
    tabColour.setBounds (224, 712, 88, 26);
    tabSpace.setBounds (316, 712, 80, 26);
    skinBtn.setBounds (kW - 16 - 150, 712, 150, 26);
    const Rectangle<int> pageR (16, 744, kW - 32, 150);
    for (auto* p : { &pageGen, &pagePlay, &pageColour, &pageSpace }) p->setBounds (pageR);

    auto place = [this] (const char* id, Rectangle<int> r) { if (auto* c = byId[id]) c->setBounds (r); };
    const int H = pageR.getHeight();
    const int kh = H - 30;              // knob height inside a group
    auto knobRow = [&] (std::initializer_list<const char*> ids, int x, int y, int w)
    {
        for (auto* id : ids) { place (id, { x, y, w, kh }); x += w; }
    };

    // GENERATOR
    {
        pageGen.groups = { { { 0, 0, pageR.getWidth(), H }, "Generator details  -  the three macros scale these down (left) or push them further (right)" } };
        const int w = (pageR.getWidth() - 16) / 16;
        knobRow ({ PID::density, PID::backbone, PID::ghosts, PID::hats, PID::chop, PID::cut16, PID::maxRepeats, PID::rolls,
                   PID::stutter, PID::reverse, PID::pitchProb, PID::pitchRange, PID::gateProb, PID::variation, PID::fillAmt, PID::humanize },
                 8, 24, w);
    }
    // PLAYBACK
    {
        pagePlay.groups = { { { 0, 0, 330, H }, "Tempo" }, { { 338, 0, 520, H }, "Voice" }, { { 866, 0, pageR.getWidth() - 866, H }, "Pattern changes" } };
        place (PID::tempoMode, { 12, 30, 200, 44 });
        place (PID::cycleMs, { 222, 24, 98, kh });
        place (PID::voiceMode, { 350, 30, 180, 44 });
        place (PID::tailMode, { 350, 84, 180, 44 });
        knobRow ({ PID::attack, PID::decay, PID::velSens }, 540, 24, 104);
        place (PID::quantize, { 878, 30, 120, 28 });
    }
    // COLOUR
    {
        pageColour.groups = { { { 0, 0, 220, H }, "" }, { { 228, 0, 340, H }, "" }, { { 576, 0, 128, H }, "" }, { { 712, 0, pageR.getWidth() - 712, H }, "" } };
        place (PID::crushOn, { 10, 8, 96, 22 });
        knobRow ({ PID::bits, PID::rate }, 10, 34, 100);
        place (PID::filterOn, { 238, 8, 96, 22 });
        place (PID::filterType, { 340, 8, 118, 22 });
        knobRow ({ PID::cutoff, PID::reso }, 258, 34, 140);
        place (PID::driveOn, { 586, 8, 96, 22 });
        knobRow ({ PID::drive }, 588, 34, 104);
        place (PID::compOn, { 722, 8, 96, 22 });
        knobRow ({ PID::compAmt, PID::compAtk, PID::compRel, PID::compMix }, 722, 34, 104);
    }
    // SPACE
    {
        pageSpace.groups = { { { 0, 0, 500, H }, "" }, { { 508, 0, 500, H }, "" }, { { 1016, 0, pageR.getWidth() - 1016, H }, "" } };
        place (PID::dlyOn, { 10, 8, 96, 22 });
        place (PID::dlyTime, { 10, 40, 116, 44 });
        place (PID::dlySrc, { 10, 92, 116, 44 });
        place (PID::dlyPing, { 132, 8, 96, 22 });
        knobRow ({ PID::dlyFb, PID::dlyTone, PID::dlyMix }, 130, 34, 120);
        place (PID::revOn, { 518, 8, 96, 22 });
        place (PID::revSrc, { 518, 40, 116, 44 });
        knobRow ({ PID::revSize, PID::revDamp, PID::revWidth, PID::revMix }, 634, 34, 92);
        knobRow ({ PID::output }, 1022, 24, pageR.getWidth() - 1028);
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
void JSliceEditor::refreshAll()
{
    using namespace juce;
    wave.refresh();
    grid.refresh();

    auto k = proc.getKit();
    nameBox.name = (k != nullptr && k->audio != nullptr) ? k->audio->name : String ("no break loaded");
    nameBox.repaint();

    const auto& st = proc.getSlicerSettings();
    sliceMode.setSelectedId (st.mode + 1, dontSendNotification);
    gridDiv.setSelectedId (st.gridDivision, dontSendNotification);
    gridDiv.setEnabled (st.mode == bc::SlicerSettings::Grid);
    for (auto* b : { &sensBox, &prerollBox, &breakBpmBox, &breakBarsBox }) b->repaint();

    if (! seedEdit.hasKeyboardFocus (true)) seedEdit.setText (String ((int) proc.getPattern().seed), false);
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
}

void JSliceEditor::pushSlicer()
{
    auto st = proc.getSlicerSettings();
    st.mode = sliceMode.getSelectedId() - 1;
    st.gridDivision = gridDiv.getSelectedId() > 0 ? gridDiv.getSelectedId() : 16;
    gridDiv.setEnabled (st.mode == bc::SlicerSettings::Grid);
    proc.setSlicerSettings (st);
}

void JSliceEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll();
}

void JSliceEditor::timerCallback()
{
    grid.tick();
    wave.tick();
    for (auto* b : { &swingBox, &pitchBox, &bpmBox, &stepsBox, &sensBox, &prerollBox, &breakBpmBox, &breakBarsBox }) b->refresh();

    const int style = (int) proc.apvts.getRawParameterValue (PID::style)->load();
    if (styleBox.getSelectedItemIndex() != style) styleBox.setSelectedItemIndex (style, juce::dontSendNotification);

    const bool running = proc.isRunning();
    const bool sync = proc.apvts.getRawParameterValue (PID::syncOn)->load() > 0.5f;
    const bool hostDriven = sync && proc.hostPlaying.load();
    playBtn.setToggleState (running, juce::dontSendNotification);
    playBtn.setButtonText (running ? (hostDriven ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0 Host")) : juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xa0 Stop")))
                                   : juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 Play")));
    const auto bpmLabel = juce::String (sync && proc.getHostBpm() > 1.0 ? "BPM  HOST" : "BPM  FREE");
    if (bpmBox.label != bpmLabel) { bpmBox.label = bpmLabel; bpmBox.repaint(); }

    const auto ep = proc.currentEngineParams();
    if (ep.patternSteps != lastSteps || ep.fillEvery != lastFill) { lastSteps = ep.patternSteps; lastFill = ep.fillEvery; grid.repaint(); }

    if (errorTicks > 0 && --errorTicks == 0) { errorText.clear(); repaint(); }
}

//==============================================================================
void JSliceEditor::loadFile (const juce::File& f)
{
    juce::String err;
    if (! proc.loadFile (f, err)) showError (err);
}

void JSliceEditor::openFileDialog()
{
    using namespace juce;
    File start (proc.settings().getValue ("lastDir"));
    chooser = std::make_unique<FileChooser> ("Load a drum break", start, JSliceProcessor::getSupportedWildcard());
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
}

void JSliceEditor::showError (const juce::String& msg)
{
    errorText = msg;
    errorTicks = 30 * 5;
    repaint();
}

void JSliceEditor::setBreakBpm (double bpm)
{
    auto k = proc.getKit();
    if (k == nullptr || bpm < 30.0 || bpm > 400.0) return;
    const int bars = bc::Analyzer::barsForTempo (k->length(), k->audio->sampleRate, bpm, k->gridOrigin);
    proc.setBreakTempo (bpm, bars, true);
}

void JSliceEditor::setBreakBars (int bars)
{
    auto k = proc.getKit();
    if (k == nullptr || bars <= 0 || bars == k->bars) return;
    const double dur = (double) (k->length() - k->gridOrigin) / k->audio->sampleRate;
    proc.setBreakTempo (bars * 240.0 / dur, bars, true);
}

bool JSliceEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;wave;aif;aiff;aifc;flac;mp3;ogg")) return true;
    return false;
}

void JSliceEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
    {
        juce::File file (f);
        if (file.hasFileExtension ("wav;wave;aif;aiff;aifc;flac;mp3;ogg")) { loadFile (file); break; }
    }
}

juce::AudioProcessorEditor* JSliceProcessor::createEditor()
{
    return new JSliceEditor (*this);
}
