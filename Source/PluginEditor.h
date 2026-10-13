#pragma once

#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Widgets.h"
#include "UI/WaveformView.h"
#include "UI/StepGrid.h"
#include "UI/BrowserPanel.h"
#include "UI/PatternBar.h"
#include <map>

/** Tab page that draws its group frames behind the controls. */
struct Page : public juce::Component
{
    void paint (juce::Graphics&) override;
    struct Group { juce::Rectangle<int> r; juce::String title; };
    std::vector<Group> groups;
};

/** "BREAK  <file name>" read-out ; click = file dialog. */
struct NameBox : public juce::Component, public juce::SettableTooltipClient
{
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    juce::String name;
    std::function<void()> onClick;
};

//==============================================================================
class JSliceEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     private juce::ChangeListener,
                     private juce::Timer
{
public:
    explicit JSliceEditor (JSliceProcessor&);
    ~JSliceEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshAll();
    void loadFile (const juce::File& f);
    void openFileDialog();
    void showTab (int t);
    void applySkin (int index);
    void pushSlicer();
    void setBreakBpm (double bpm);
    void setBreakBars (int bars);
    void showError (const juce::String& msg);

    ui::Knob& knob (juce::Component& page, const char* id, const juce::String& caption);
    ui::ParamChoice& choice (juce::Component& page, const char* id, const juce::String& caption);
    ui::ParamToggle& toggle (juce::Component& page, const char* id, const juce::String& text);

    JSliceProcessor& proc;
    ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };

    // header
    ui::ValueBox swingBox { "Swing" }, pitchBox { "Pitch" }, bpmBox { "BPM" };
    NameBox nameBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, browseBtn { "Browse" }, playBtn { "Play" };
    ui::ParamToggle syncBtn;

    // break display + slicer
    WaveformView wave;
    juce::ComboBox sliceMode, gridDiv;
    ui::ValueBox sensBox { "Sensitivity" }, prerollBox { "Pre-roll" }, breakBpmBox { "Break BPM" }, breakBarsBox { "Bars" };
    juce::TextButton halfBtn { "/2" }, dblBtn { "x2" }, detectBtn { "Auto" };
    juce::ToggleButton keepPattern { "Keep groove on load" };

    // sequencer
    StepGrid grid;
    PatternBar patternBar { proc };
    ui::ParamToggle seqBtn;
    juce::TextButton minusBtn { "-" }, plusBtn { "+" };
    ui::ValueBox stepsBox { "Length" };
    ui::ParamChoice fillEvery;
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" }, clearLocksBtn { "-Locks" };
    juce::TextEditor seedEdit;
    juce::OwnedArray<juce::TextButton> modeBtns;   // Notes, Delay, Reverb, Lo-fi, Filter
    juce::TextButton clearFxBtn { "Clear FX" };

    // generator panel
    juce::ComboBox styleBox;
    juce::TextButton generateBtn { "Generate" }, mutateBtn { "Mutate" }, fillBtn { "Fill!" };
    std::unique_ptr<ui::Knob> complexityKnob, energyKnob, chaosKnob;

    // bottom tabs
    juce::TextButton tabGen { "Generator" }, tabPlay { "Playback" }, tabColour { "Colour" }, tabSpace { "Space" }, skinBtn { "Skin" };
    Page pageGen, pagePlay, pageColour, pageSpace;

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::map<juce::String, juce::Component*> byId;

    std::unique_ptr<BrowserPanel> browser;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String errorText;
    int errorTicks = 0;
    int lastSteps = -1, lastFill = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JSliceEditor)
};
