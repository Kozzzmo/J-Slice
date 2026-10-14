#pragma once

#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Widgets.h"
#include "UI/WaveformView.h"
#include "UI/StepGrid.h"
#include "UI/Inspector.h"
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

/** Identity card of one break : letter, tempo, bars, slices, file name, previous / next file (and ON for B). */
struct BreakCard : public juce::Component, public juce::SettableTooltipClient
{
    BreakCard (JSliceProcessor& p, int slot);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }

    JSliceProcessor& proc;
    const int slot;
    bool active = false;
    std::function<void()> onClick;
    std::function<void (int delta)> onSibling;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    std::unique_ptr<ui::ParamToggle> onBtn;
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
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshAll();
    void loadFile (int slot, const juce::File& f);
    void openFileDialog (int slot);
    void setActiveSlot (int slot);
    void showTab (int t);
    void applySkin (int index);
    void pushSlicer();
    void setBreakBpm (double bpm);
    void setBreakBars (int bars);
    void showError (const juce::String& msg);
    void refreshZoom();
    void mouseUp (const juce::MouseEvent&) override;
    void showUpdateDialog();

    ui::Knob& knob (juce::Component& page, const char* id, const juce::String& caption);
    ui::ParamChoice& choice (juce::Component& page, const char* id, const juce::String& caption);
    ui::ParamToggle& toggle (juce::Component& page, const char* id, const juce::String& text);

    JSliceProcessor& proc;
    ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 700 };
    int activeSlot = 0;

    // header
    ui::ValueBox swingBox { "Swing" }, pitchBox { "Pitch" }, bpmBox { "BPM" };
    juce::TextButton browseBtn { "Browse" }, playBtn { "Play" }, skinBtn { "Skin" };
    ui::ParamToggle syncBtn;

    // the two breaks
    BreakCard cardA, cardB;
    WaveformView waveA, waveB;

    // slicer row (edits the active break)
    juce::ComboBox sliceMode, gridDiv;
    ui::ValueBox sensBox { "Sensitivity" }, prerollBox { "Pre-roll" }, breakBpmBox { "Break BPM" }, breakBarsBox { "Bars" };
    juce::TextButton halfBtn { "/2" }, dblBtn { "x2" }, detectBtn { "Auto" };
    juce::ToggleButton keepPattern { "Keep groove on load" };

    // pattern slots + chain
    PatternBar patternBar { proc };

    // lanes
    StepGrid grid;
    Inspector inspector;
    juce::TextButton pagePrev { "<" }, pageNext { ">" }, fillViewBtn { "Fill" }, undoBtn { "Undo" }, redoBtn { "Redo" };
    juce::OwnedArray<juce::TextButton> zoomBtns;
    ui::ParamChoice duoBox;
    ui::ValueBox shareBox { "B share" }, stepsBox { "Length" };

    // generator
    juce::ComboBox styleBox;
    juce::TextButton generateBtn { "Generate" }, mutateBtn { "Mutate" }, fillBtn { "Fill!" };
    ui::ParamToggle autoFillBtn;
    ui::ParamChoice fillEvery;
    std::unique_ptr<ui::ZoneFader> complexityFader, energyFader, chaosFader;

    // bottom tabs
    juce::TextButton tabGen { "Details" }, tabPlay { "Playback" }, tabColour { "Colour" }, tabSpace { "Space" };
    Page pageGen, pagePlay, pageColour, pageSpace;
    juce::TextEditor seedEdit;
    juce::TextButton clearLocksBtn { "-Locks" }, clearFxBtn { "-FX" };

    std::vector<std::unique_ptr<juce::Component>> owned;
    std::map<juce::String, juce::Component*> byId;

    std::unique_ptr<BrowserPanel> browser;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String errorText;
    int errorTicks = 0;
    int lastSteps = -1, lastFill = -1;
    bool lastBActive = false;
    juce::String lastPageTitle;
    int lastUpdState = -1;
    bool updWaiting = false;   // "Check now" pressed: show the result when it arrives

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JSliceEditor)
};
