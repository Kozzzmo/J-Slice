#pragma once

#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/WaveformView.h"
#include "UI/StepGrid.h"
#include "UI/BrowserPanel.h"
#include <map>

//==============================================================================
/** Rotary knob with a caption and a value read-out, attached to a parameter. */
struct Knob : public juce::Component
{
    Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption, bool big = false);
    void resized() override;
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    bool isBig;
};

/** Labelled combo box attached to a choice parameter. */
struct Choice : public juce::Component
{
    Choice (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption);
    void resized() override;
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

/** LED toggle attached to a bool parameter. */
struct Toggle : public juce::Component
{
    Toggle (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& caption);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

/** Pad you drag into Ableton : creates the file on the fly. */
struct DragTile : public juce::Component, public juce::SettableTooltipClient
{
    DragTile (const juce::String& title, const juce::String& sub, std::function<juce::File()> make);
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    juce::String title, sub;
    std::function<juce::File()> makeFile;
    bool dragging = false;
};

/** Tab page that draws titled groups behind its controls. */
struct Page : public juce::Component
{
    void paint (juce::Graphics&) override;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> groups;
};

//==============================================================================
class JSliceEditor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     public juce::DragAndDropContainer,
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
    void showTab (int t);
    void setBreakBpm (double bpm);
    void setBreakBars (int bars);
    void showError (const juce::String& msg);

    JSliceProcessor& proc;
    ui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // header
    juce::TextButton browseBtn { "BROWSE" }, loadBtn { "LOAD" }, prevBtn { "<" }, nextBtn { ">" };
    juce::Label sampleName;
    juce::Label bpmLabel, bpmCaption, barsCaption;
    juce::TextButton halfBtn { "/2" }, dblBtn { "x2" }, detectBtn { "AUTO" };
    juce::ComboBox breakBars;
    juce::TextButton playBtn { "PLAY" }, seqBtn { "SEQ" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqAtt;

    // break display + slicer
    WaveformView wave;
    juce::ComboBox sliceMode, gridDiv;
    juce::Slider sensitivity, preroll;
    juce::Label sliceModeCap, sensCap, gridCap, preCap, sliceHint;

    // sequencer
    StepGrid grid;
    juce::TextButton generateBtn { "GENERATE" }, mutateBtn { "MUTATE" }, fillBtn { "FILL!" }, undoBtn { "UNDO" }, redoBtn { "REDO" }, clearLocksBtn { "UNLOCK ALL" };
    juce::Label seedCap;
    juce::TextEditor seedEdit;
    DragTile dragMidi, dragAudio;
    juce::ToggleButton keepPattern { "Keep groove when loading a new break" };

    // bottom tabs
    juce::TextButton tabMain { "MAIN" }, tabGen { "GENERATOR" }, tabSound { "SOUND" };
    Page pageMain, pageGen, pageSound;
    int currentTab = 0;

    juce::ComboBox styleBox;
    juce::Label styleCap;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Choice>> choices;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::map<juce::String, juce::Component*> byId;

    std::unique_ptr<BrowserPanel> browser;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String errorText;
    int errorTicks = 0;
    int lastBars = -1, lastFill = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JSliceEditor)
};
