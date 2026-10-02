#include "PluginEditor.h"

using namespace ui;

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption, bool big)
    : isBig (big)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 15);
    slider.setColour (juce::Slider::textBoxTextColourId, col::amber);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setDoubleClickReturnValue (true, (double) s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (slider);
    label.setText (caption.toUpperCase(), juce::dontSendNotification);
    label.setFont (labelFont (big ? 12.5f : 10.5f));
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, col::cream);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (isBig ? 18 : 14));
    slider.setBounds (r);
}

Choice::Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    label.setText (caption.toUpperCase(), juce::dontSendNotification);
    label.setFont (labelFont (10.5f));
    label.setColour (juce::Label::textColourId, col::cream);
    addAndMakeVisible (label);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (15));
    box.setBounds (r.removeFromTop (juce::jmin (26, r.getHeight())));
}

Toggle::Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption)
{
    button.setButtonText (caption);
    addAndMakeVisible (button);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, id, button);
}

DragTile::DragTile (const juce::String& t, const juce::String& s, std::function<juce::File()> make)
    : title (t), sub (s), makeFile (std::move (make))
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void DragTile::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    g.setColour (dragging ? col::accent.withAlpha (0.25f) : col::panelEdge);
    g.fillRoundedRectangle (r, 5.0f);
    const float dash[] = { 4.0f, 3.0f };
    Path p; p.addRoundedRectangle (r, 5.0f);
    PathStrokeType (1.2f).createDashedStroke (p, p, dash, 2);
    g.setColour (col::accent);
    g.fillPath (p);
    g.setFont (labelFont (13.0f));
    g.setColour (col::cream);
    g.drawText (title, r.withTrimmedBottom (r.getHeight() * 0.45f).toNearestInt(), Justification::centredBottom);
    g.setFont (labelFont (10.0f, false));
    g.setColour (col::dim);
    g.drawText (sub, r.withTrimmedTop (r.getHeight() * 0.55f).toNearestInt(), Justification::centredTop);
}

void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    dragging = true;
    repaint();
    const auto f = makeFile();
    if (f.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this,
            [safe = juce::Component::SafePointer<DragTile> (this)] { if (safe != nullptr) { safe->dragging = false; safe->repaint(); } });
    else { dragging = false; repaint(); }
}

void DragTile::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    repaint();
}

void Page::paint (juce::Graphics& g)
{
    for (auto& [r, title] : groups)
    {
        g.setColour (col::panelEdge.withAlpha (0.55f));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setFont (labelFont (10.5f));
        g.setColour (col::accent);
        g.drawText (title.toUpperCase(), r.getX() + 8, r.getY() + 3, r.getWidth() - 16, 13, juce::Justification::centredLeft);
    }
}

//==============================================================================
JSliceEditor::JSliceEditor (JSliceProcessor& p)
    : AudioProcessorEditor (&p), proc (p), wave (p), grid (p),
      dragMidi ("DRAG MIDI", "clip -> any track", [&p] { return p.exportMidi(); }),
      dragAudio ("DRAG AUDIO", "rendered loop .wav", [&p] { return p.exportAudio(); })
{
    using namespace juce;
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    // ---- header
    for (auto* b : { &browseBtn, &loadBtn, &prevBtn, &nextBtn, &halfBtn, &dblBtn, &detectBtn, &playBtn, &seqBtn })
        addAndMakeVisible (b);
    browseBtn.setClickingTogglesState (true);
    browseBtn.setTooltip ("Open the sample browser (favourite folders, load on click)");
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
    loadBtn.setTooltip ("Open a file dialog");
    loadBtn.onClick = [this]
    {
        File start (proc.settings().getValue ("lastDir"));
        chooser = std::make_unique<FileChooser> ("Load a drum break", start, JSliceProcessor::getSupportedWildcard());
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc) { if (fc.getResult().existsAsFile()) loadFile (fc.getResult()); });
    };
    prevBtn.setTooltip ("Previous file in the same folder");
    nextBtn.setTooltip ("Next file in the same folder");
    prevBtn.onClick = [this] { auto f = proc.getSiblingFile (-1); if (f.existsAsFile()) loadFile (f); };
    nextBtn.onClick = [this] { auto f = proc.getSiblingFile (1);  if (f.existsAsFile()) loadFile (f); };

    sampleName.setFont (monoFont (14.0f));
    sampleName.setColour (Label::textColourId, col::lcdInk);
    sampleName.setColour (Label::backgroundColourId, col::lcdBg);
    sampleName.setJustificationType (Justification::centredLeft);
    addAndMakeVisible (sampleName);

    bpmCaption.setText ("BREAK BPM", dontSendNotification);
    barsCaption.setText ("BARS", dontSendNotification);
    for (auto* l : { &bpmCaption, &barsCaption })
    {
        l->setFont (labelFont (10.0f));
        l->setColour (Label::textColourId, col::dim);
        l->setJustificationType (Justification::centred);
        addAndMakeVisible (l);
    }
    bpmLabel.setEditable (true);
    bpmLabel.setFont (monoFont (17.0f));
    bpmLabel.setColour (Label::textColourId, col::lcdInk);
    bpmLabel.setColour (Label::backgroundColourId, col::lcdBg);
    bpmLabel.setColour (Label::textWhenEditingColourId, col::amber);
    bpmLabel.setColour (Label::backgroundWhenEditingColourId, col::panelEdge);
    bpmLabel.setJustificationType (Justification::centred);
    bpmLabel.setTooltip ("Original tempo of the break (detected). Double-click to type a value.");
    bpmLabel.onTextChange = [this] { setBreakBpm (bpmLabel.getText().getDoubleValue()); };
    addAndMakeVisible (bpmLabel);
    halfBtn.setTooltip ("Halve the break tempo");
    dblBtn.setTooltip ("Double the break tempo");
    detectBtn.setTooltip ("Back to the automatically detected tempo");
    halfBtn.onClick = [this] { if (auto k = proc.getKit()) proc.setBreakTempo (k->bpm * 0.5, juce::jmax (1, k->bars / 2), true); };
    dblBtn.onClick  = [this] { if (auto k = proc.getKit()) proc.setBreakTempo (k->bpm * 2.0, k->bars * 2, true); };
    detectBtn.onClick = [this] { proc.redetectTempo(); };
    for (int b : { 1, 2, 3, 4, 6, 8, 12, 16 }) breakBars.addItem (String (b), b);
    breakBars.setTooltip ("Length of the break in bars (the tempo follows)");
    breakBars.onChange = [this] { setBreakBars (breakBars.getSelectedId()); };
    addAndMakeVisible (breakBars);

    playBtn.setClickingTogglesState (true);
    playBtn.setTooltip ("Preview without starting the DAW (uses the project tempo)");
    playBtn.onClick = [this] { proc.internalPlay.store (playBtn.getToggleState()); };
    seqBtn.setClickingTogglesState (true);
    seqBtn.setTooltip ("Sequencer on/off. Off = play the slices from MIDI notes (C1 = slice 1)");
    seqAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, PID::seqOn, seqBtn);

    // ---- break display + slicer
    addAndMakeVisible (wave);
    sliceMode.addItemList ({ "Smart", "Transient", "Grid" }, 1);
    sliceMode.setTooltip ("Smart: the musically important hits (grid aware, flams merged, ghosts kept when strong)\n"
                          "Transient: every transient above the sensitivity\nGrid: equal divisions snapped to nearby hits");
    gridDiv.addItem ("1/4", 4); gridDiv.addItem ("1/8", 8); gridDiv.addItem ("1/16", 16); gridDiv.addItem ("1/32", 32);
    for (auto* sl : { &sensitivity, &preroll })
    {
        sl->setSliderStyle (Slider::LinearHorizontal);
        sl->setTextBoxStyle (Slider::TextBoxRight, false, 58, 20);
        sl->setColour (Slider::textBoxTextColourId, col::amber);
        sl->setColour (Slider::textBoxBackgroundColourId, col::panelEdge);
        sl->setColour (Slider::textBoxOutlineColourId, Colour (0xff4a463e));
        addAndMakeVisible (sl);
    }
    sensitivity.setRange (0.0, 1.0, 0.01);
    sensitivity.setTooltip ("More = more slices (ghost notes, hats). Less = only the main hits.");
    preroll.setRange (0.0, 10.0, 0.1);
    preroll.setTextValueSuffix (" ms");
    preroll.setTooltip ("Cut this much before each transient so the attack stays intact");

    auto pushSlicer = [this]
    {
        auto st = proc.getSlicerSettings();
        st.mode = sliceMode.getSelectedId() - 1;
        st.gridDivision = gridDiv.getSelectedId();
        st.sensitivity = (float) sensitivity.getValue();
        st.prerollMs = (float) preroll.getValue();
        gridDiv.setEnabled (st.mode == bc::SlicerSettings::Grid);
        proc.setSlicerSettings (st);
    };
    sliceMode.onChange = pushSlicer;
    gridDiv.onChange = pushSlicer;
    sensitivity.onDragEnd = pushSlicer;
    preroll.onDragEnd = pushSlicer;
    sensitivity.onValueChange = [this, pushSlicer] { if (! sensitivity.isMouseButtonDown()) pushSlicer(); };
    preroll.onValueChange = [this, pushSlicer] { if (! preroll.isMouseButtonDown()) pushSlicer(); };
    addAndMakeVisible (sliceMode);
    addAndMakeVisible (gridDiv);

    for (auto [l, t] : { std::pair<Label*, const char*> { &sliceModeCap, "SLICER" }, { &sensCap, "SENSITIVITY" }, { &gridCap, "GRID" }, { &preCap, "PRE-ROLL" } })
    {
        l->setText (t, dontSendNotification);
        l->setFont (labelFont (10.5f));
        l->setColour (Label::textColourId, col::cream);
        l->setJustificationType (Justification::centredRight);
        addAndMakeVisible (l);
    }
    sliceHint.setText ("Click: listen  |  drag marker: move  |  double-click: add  |  right-click: class / delete", dontSendNotification);
    sliceHint.setFont (labelFont (10.5f, false));
    sliceHint.setColour (Label::textColourId, col::dim);
    sliceHint.setJustificationType (Justification::centredRight);
    addAndMakeVisible (sliceHint);

    // ---- sequencer
    addAndMakeVisible (grid);
    for (auto* b : { &generateBtn, &mutateBtn, &fillBtn, &undoBtn, &redoBtn, &clearLocksBtn })
        addAndMakeVisible (b);
    generateBtn.setColour (TextButton::buttonColourId, col::accent);
    generateBtn.setColour (TextButton::textColourOffId, col::body);
    generateBtn.setTooltip ("New pattern (locked steps are kept). MIDI: C0 or the 'Trigger: generate' parameter");
    mutateBtn.setTooltip ("Small variations of the current pattern. MIDI: C#0");
    fillBtn.setTooltip ("Play the fill bar at the next bar. MIDI: D0");
    undoBtn.setTooltip ("Undo (MIDI: D#0)");
    generateBtn.onClick = [this] { proc.generate(); };
    mutateBtn.onClick = [this] { proc.mutate(); };
    fillBtn.onClick = [this] { proc.requestFill(); };
    undoBtn.onClick = [this] { proc.undo(); };
    redoBtn.onClick = [this] { proc.redo(); };
    clearLocksBtn.onClick = [this] { proc.clearLocks(); };

    seedCap.setText ("SEED", dontSendNotification);
    seedCap.setFont (labelFont (10.5f));
    seedCap.setColour (Label::textColourId, col::cream);
    addAndMakeVisible (seedCap);
    seedEdit.setFont (monoFont (14.0f));
    seedEdit.setInputRestrictions (9, "0123456789");
    seedEdit.setJustification (Justification::centred);
    seedEdit.setTooltip ("Same seed + same settings = same pattern. Type a number and press Enter.");
    seedEdit.onReturnKey = [this] { proc.generate ((uint32_t) seedEdit.getText().getLargeIntValue()); unfocusAllComponents(); };
    addAndMakeVisible (seedEdit);
    dragMidi.setTooltip ("Drag onto an Ableton MIDI track (notes C1 = slice 1). Turn SEQ off on this track to play it with J-Slice.");
    dragAudio.setTooltip ("Drag the rendered loop (with pitch, reverse, rolls and FX) onto an audio track");
    addAndMakeVisible (dragMidi);
    addAndMakeVisible (dragAudio);
    keepPattern.setToggleState (proc.keepPatternOnLoad(), dontSendNotification);
    keepPattern.onClick = [this] { proc.setKeepPatternOnLoad (keepPattern.getToggleState()); };
    keepPattern.setTooltip ("When loading another break, keep the groove and map each hit to the same kind of drum");
    addAndMakeVisible (keepPattern);

    // ---- tabs
    for (auto* b : { &tabMain, &tabGen, &tabSound })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        addAndMakeVisible (b);
    }
    tabMain.onClick = [this] { showTab (0); };
    tabGen.onClick = [this] { showTab (1); };
    tabSound.onClick = [this] { showTab (2); };
    addAndMakeVisible (pageMain);
    addChildComponent (pageGen);
    addChildComponent (pageSound);

    auto knob = [&] (Component& page, const char* id, const String& cap, bool big = false)
    {
        knobs.push_back (std::make_unique<Knob> (s, id, cap, big));
        page.addAndMakeVisible (*knobs.back());
        byId[id] = knobs.back().get();
    };
    auto choice = [&] (Component& page, const char* id, const String& cap)
    {
        choices.push_back (std::make_unique<Choice> (s, id, cap));
        page.addAndMakeVisible (*choices.back());
        byId[id] = choices.back().get();
    };
    auto toggle = [&] (Component& page, const char* id, const String& cap)
    {
        toggles.push_back (std::make_unique<Toggle> (s, id, cap));
        page.addAndMakeVisible (*toggles.back());
        byId[id] = toggles.back().get();
    };

    // MAIN
    styleBox.addItemList (bc::getStyleNames(), 1);
    styleBox.setTooltip ("Loads the style's settings and generates a new pattern");
    styleBox.onChange = [this] { proc.applyStyle (styleBox.getSelectedItemIndex()); };
    pageMain.addAndMakeVisible (styleBox);
    styleCap.setText ("STYLE", dontSendNotification);
    styleCap.setFont (labelFont (10.5f));
    styleCap.setColour (Label::textColourId, col::cream);
    pageMain.addAndMakeVisible (styleCap);
    choice (pageMain, PID::bars, "Pattern length (bars)");
    choice (pageMain, PID::fillEvery, "Fill");
    knob (pageMain, PID::complexity, "Complexity", true);
    knob (pageMain, PID::energy, "Energy", true);
    knob (pageMain, PID::chaos, "Chaos", true);
    knob (pageMain, PID::swing, "Swing");
    knob (pageMain, PID::pitch, "Pitch");
    choice (pageMain, PID::tempoMode, "Tempo mode");
    knob (pageMain, PID::cycleMs, "Cycle");
    toggle (pageMain, PID::quantize, "Changes on next bar");

    // GENERATOR
    const std::pair<const char*, const char*> genKnobs[] = {
        { PID::density, "Density" }, { PID::backbone, "Backbone" }, { PID::ghosts, "Ghosts" }, { PID::hats, "Hats" },
        { PID::chop, "Chop" }, { PID::cut16, "16th cuts" }, { PID::maxRepeats, "Repeats" }, { PID::rolls, "Rolls" },
        { PID::stutter, "Stutter" }, { PID::reverse, "Reverse" }, { PID::pitchProb, "Pitch prob" }, { PID::pitchRange, "Pitch range" },
        { PID::gateProb, "Short hits" }, { PID::variation, "Variation" }, { PID::fillAmt, "Fill amount" }, { PID::humanize, "Humanize" } };
    for (auto& [id, cap] : genKnobs) knob (pageGen, id, cap);

    // SOUND
    choice (pageSound, PID::voiceMode, "Voices");
    choice (pageSound, PID::tailMode, "Slice tail");
    knob (pageSound, PID::attack, "Attack");
    knob (pageSound, PID::decay, "Tightness");
    knob (pageSound, PID::velSens, "Velocity");
    toggle (pageSound, PID::crushOn, "On");
    knob (pageSound, PID::bits, "Bits");
    knob (pageSound, PID::rate, "Rate");
    toggle (pageSound, PID::filterOn, "On");
    choice (pageSound, PID::filterType, "Type");
    knob (pageSound, PID::cutoff, "Cutoff");
    knob (pageSound, PID::reso, "Reso");
    toggle (pageSound, PID::driveOn, "On");
    knob (pageSound, PID::drive, "Drive");
    knob (pageSound, PID::output, "Output");

    tabMain.setToggleState (true, dontSendNotification);
    showTab (0);

    proc.addChangeListener (this);
    setSize (1180, 800);
    refreshAll();
    startTimerHz (30);
}

JSliceEditor::~JSliceEditor()
{
    proc.removeChangeListener (this);
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void JSliceEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (col::body);

    // brushed texture
    Random rng (7);
    for (int i = 0; i < 160; ++i)
    {
        g.setColour (Colours::white.withAlpha (0.012f));
        const int y = rng.nextInt (getHeight());
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());
    }

    // logo
    g.setFont (Font (FontOptions (30.0f, Font::bold | Font::italic)));
    g.setColour (col::accent);
    g.drawText ("J-SLICE", 16, 8, 140, 32, Justification::left);
    g.setFont (labelFont (9.5f));
    g.setColour (col::dim);
    g.drawText ("JUNGLE BREAK SLICER / GENERATOR", 16, 38, 220, 12, Justification::left);

    drawPanel (g, { 8, 62, getWidth() - 16, 230 }, "");
    drawPanel (g, { 8, 298, getWidth() - 16, 268 }, "Sequencer");
    drawPanel (g, { 8, 572, getWidth() - 16, getHeight() - 580 }, "");

    if (errorText.isNotEmpty())
    {
        g.setColour (Colours::red.withAlpha (0.85f));
        g.setFont (labelFont (12.0f));
        g.drawText (errorText, 250, 44, 600, 14, Justification::left);
    }
}

void JSliceEditor::resized()
{
    using namespace juce;
    // ---- header
    browseBtn.setBounds (240, 12, 84, 30);
    loadBtn.setBounds (328, 12, 60, 30);
    prevBtn.setBounds (394, 12, 28, 30);
    sampleName.setBounds (424, 12, 300, 30);
    nextBtn.setBounds (726, 12, 28, 30);
    bpmCaption.setBounds (764, 2, 96, 12);
    bpmLabel.setBounds (764, 14, 96, 28);
    halfBtn.setBounds (862, 14, 32, 28);
    dblBtn.setBounds (896, 14, 32, 28);
    barsCaption.setBounds (934, 2, 58, 12);
    breakBars.setBounds (934, 14, 58, 28);
    detectBtn.setBounds (996, 14, 52, 28);
    playBtn.setBounds (1058, 12, 56, 30);
    seqBtn.setBounds (1116, 12, 52, 30);

    // ---- break display
    wave.setBounds (16, 70, getWidth() - 32, 178);
    int x = 16;
    const int y = 256;
    sliceModeCap.setBounds (x, y, 50, 28);            x += 54;
    sliceMode.setBounds (x, y + 2, 110, 24);          x += 118;
    sensCap.setBounds (x, y, 82, 28);                 x += 86;
    sensitivity.setBounds (x, y + 2, 190, 24);        x += 198;
    gridCap.setBounds (x, y, 36, 28);                 x += 40;
    gridDiv.setBounds (x, y + 2, 70, 24);             x += 78;
    preCap.setBounds (x, y, 62, 28);                  x += 66;
    preroll.setBounds (x, y + 2, 150, 24);            x += 156;
    sliceHint.setBounds (x, y, getWidth() - 16 - x, 28);

    // ---- sequencer
    grid.setBounds (16, 318, 860, 240);
    const int cx = 890, cw = getWidth() - 16 - cx;
    generateBtn.setBounds (cx, 318, cw, 42);
    mutateBtn.setBounds (cx, 364, cw / 2 - 2, 30);
    fillBtn.setBounds (cx + cw / 2 + 2, 364, cw / 2 - 2, 30);
    undoBtn.setBounds (cx, 398, cw / 3 - 3, 26);
    redoBtn.setBounds (cx + cw / 3, 398, cw / 3 - 3, 26);
    clearLocksBtn.setBounds (cx + 2 * cw / 3, 398, cw / 3, 26);
    seedCap.setBounds (cx, 430, 44, 26);
    seedEdit.setBounds (cx + 46, 430, cw - 46, 26);
    dragMidi.setBounds (cx, 462, cw / 2 - 3, 60);
    dragAudio.setBounds (cx + cw / 2 + 3, 462, cw / 2 - 3, 60);
    keepPattern.setBounds (cx, 528, cw, 24);

    // ---- tabs + pages
    tabMain.setBounds (20, 580, 90, 24);
    tabGen.setBounds (114, 580, 110, 24);
    tabSound.setBounds (228, 580, 90, 24);
    const Rectangle<int> pageR (16, 608, getWidth() - 32, getHeight() - 616);
    for (auto* p : { &pageMain, &pageGen, &pageSound }) p->setBounds (pageR);

    auto place = [this] (const char* id, Rectangle<int> r) { if (auto* c = byId[id]) c->setBounds (r); };

    // MAIN
    {
        pageMain.groups = { { { 0, 0, 300, 176 }, "Style + pattern" }, { { 308, 0, 400, 176 }, "Macros" }, { { 716, 0, 432, 176 }, "Groove + tempo" } };
        styleCap.setBounds (10, 20, 100, 14);
        styleBox.setBounds (10, 36, 280, 32);
        place (PID::bars, { 10, 82, 135, 44 });
        place (PID::fillEvery, { 155, 82, 135, 44 });
        place (PID::complexity, { 318, 22, 124, 148 });
        place (PID::energy, { 446, 22, 124, 148 });
        place (PID::chaos, { 574, 22, 124, 148 });
        place (PID::swing, { 724, 22, 86, 100 });
        place (PID::pitch, { 812, 22, 86, 100 });
        place (PID::tempoMode, { 904, 26, 236, 44 });
        place (PID::cycleMs, { 904, 76, 86, 96 });
        place (PID::quantize, { 1000, 110, 148, 24 });
    }
    // GENERATOR
    {
        pageGen.groups = { { { 0, 0, getWidth() - 32, 176 }, "Generator details (the macros scale these)" } };
        int i = 0;
        for (auto id : { PID::density, PID::backbone, PID::ghosts, PID::hats, PID::chop, PID::cut16, PID::maxRepeats, PID::rolls,
                         PID::stutter, PID::reverse, PID::pitchProb, PID::pitchRange, PID::gateProb, PID::variation, PID::fillAmt, PID::humanize })
        {
            const int row = i / 8, colI = i % 8;
            place (id, { 10 + colI * 141, 18 + row * 78, 130, 78 });
            ++i;
        }
    }
    // SOUND
    {
        pageSound.groups = { { { 0, 0, 380, 176 }, "Voice" }, { { 388, 0, 220, 176 }, "Vintage sampler" },
                             { { 616, 0, 300, 176 }, "Filter" }, { { 924, 0, 224, 176 }, "Drive + output" } };
        place (PID::voiceMode, { 10, 22, 170, 44 });
        place (PID::tailMode, { 190, 22, 180, 44 });
        place (PID::attack, { 10, 76, 110, 96 });
        place (PID::decay, { 130, 76, 110, 96 });
        place (PID::velSens, { 250, 76, 110, 96 });
        place (PID::crushOn, { 398, 22, 80, 24 });
        place (PID::bits, { 398, 60, 100, 110 });
        place (PID::rate, { 502, 60, 100, 110 });
        place (PID::filterOn, { 626, 22, 60, 24 });
        place (PID::filterType, { 690, 18, 216, 44 });
        place (PID::cutoff, { 640, 70, 120, 100 });
        place (PID::reso, { 770, 70, 120, 100 });
        place (PID::driveOn, { 934, 22, 60, 24 });
        place (PID::drive, { 934, 60, 100, 110 });
        place (PID::output, { 1040, 60, 100, 110 });
    }

    if (browser != nullptr)
        browser->setBounds (12, 52, 420, getHeight() - 64);
}

void JSliceEditor::showTab (int t)
{
    currentTab = t;
    pageMain.setVisible (t == 0);
    pageGen.setVisible (t == 1);
    pageSound.setVisible (t == 2);
}

//==============================================================================
void JSliceEditor::refreshAll()
{
    using namespace juce;
    wave.refresh();
    grid.refresh();

    auto k = proc.getKit();
    if (k != nullptr && k->audio != nullptr)
    {
        sampleName.setText (" " + k->audio->name, dontSendNotification);
        if (! bpmLabel.isBeingEdited()) bpmLabel.setText (String (k->bpm, 2), dontSendNotification);
        if (breakBars.indexOfItemId (k->bars) < 0) breakBars.addItem (String (k->bars), k->bars);
        breakBars.setSelectedId (k->bars, dontSendNotification);
    }
    else
    {
        sampleName.setText (" no break loaded", dontSendNotification);
        bpmLabel.setText ("--", dontSendNotification);
    }

    const auto& st = proc.getSlicerSettings();
    sliceMode.setSelectedId (st.mode + 1, dontSendNotification);
    gridDiv.setSelectedId (st.gridDivision, dontSendNotification);
    gridDiv.setEnabled (st.mode == bc::SlicerSettings::Grid);
    if (! sensitivity.isMouseButtonDown()) sensitivity.setValue (st.sensitivity, dontSendNotification);
    if (! preroll.isMouseButtonDown()) preroll.setValue (st.prerollMs, dontSendNotification);

    if (! seedEdit.hasKeyboardFocus (true)) seedEdit.setText (String ((int) proc.getPattern().seed), false);
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
}

void JSliceEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll();
}

void JSliceEditor::timerCallback()
{
    grid.tick();
    wave.tick();

    const int style = (int) proc.apvts.getRawParameterValue (PID::style)->load();
    if (styleBox.getSelectedItemIndex() != style) styleBox.setSelectedItemIndex (style, juce::dontSendNotification);

    const bool hostPlays = proc.hostPlaying.load();
    playBtn.setButtonText (hostPlays ? "HOST" : "PLAY");
    if (hostPlays && playBtn.getToggleState()) { playBtn.setToggleState (false, juce::dontSendNotification); proc.internalPlay.store (false); }

    // settings that change what the grid shows (pattern length / fill)
    const auto ep = proc.currentEngineParams();
    if (ep.patternBars != lastBars || ep.fillEvery != lastFill) { lastBars = ep.patternBars; lastFill = ep.fillEvery; grid.repaint(); }

    if (errorTicks > 0 && --errorTicks == 0) { errorText.clear(); repaint(); }
}

//==============================================================================
void JSliceEditor::loadFile (const juce::File& f)
{
    juce::String err;
    if (! proc.loadFile (f, err)) showError (err);
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
    if (k == nullptr || bpm < 30.0 || bpm > 400.0) { refreshAll(); return; }
    const int bars = bc::Analyzer::barsForTempo (k->length(), k->audio->sampleRate, bpm, k->gridOrigin);
    proc.setBreakTempo (bpm, bars, true);
}

void JSliceEditor::setBreakBars (int bars)
{
    auto k = proc.getKit();
    if (k == nullptr || bars <= 0) return;
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
