#pragma once

#include "LookAndFeel.h"

class JSliceProcessor;

/** Slide-in sample browser : favourite folders, quick locations, load-on-click (arrow keys browse fast). */
class BrowserPanel : public juce::Component, private juce::FileBrowserListener
{
public:
    BrowserPanel (JSliceProcessor& p, std::function<void (const juce::File&)> onLoad, std::function<void()> onClose);
    ~BrowserPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void showFile (const juce::File& f);

private:
    void selectionChanged() override;
    void fileClicked (const juce::File&, const juce::MouseEvent&) override {}
    void fileDoubleClicked (const juce::File& f) override;
    void browserRootChanged (const juce::File& newRoot) override;
    void refreshFavourites();
    void setRoot (const juce::File& dir);

    JSliceProcessor& proc;
    std::function<void (const juce::File&)> onLoad;
    std::function<void()> onClose;

    juce::WildcardFileFilter filter;
    std::unique_ptr<juce::FileBrowserComponent> browser;
    juce::TextButton closeBtn { "X" }, addFav { "+ FAV" }, removeFav { "- FAV" };
    juce::ComboBox favBox;
    juce::OwnedArray<juce::TextButton> quick;
    juce::Array<juce::File> quickDirs;
    juce::ToggleButton autoLoad { "Load on click (use arrow keys)" };
    juce::File lastLoaded;
};
