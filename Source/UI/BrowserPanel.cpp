#include "BrowserPanel.h"
#include "../PluginProcessor.h"

using namespace ui;

BrowserPanel::BrowserPanel (JSliceProcessor& p, std::function<void (const juce::File&)> load, std::function<void()> close)
    : proc (p), onLoad (std::move (load)), onClose (std::move (close)),
      filter (JSliceProcessor::getSupportedWildcard(), "*", "Audio files")
{
    using namespace juce;
    File start (proc.settings().getValue ("lastDir"));
    if (! start.isDirectory()) start = File::getSpecialLocation (File::userMusicDirectory);
    if (! start.isDirectory()) start = File::getSpecialLocation (File::userHomeDirectory);

    browser = std::make_unique<FileBrowserComponent> (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles
                                                      | FileBrowserComponent::filenameBoxIsReadOnly,
                                                      start, &filter, nullptr);
    browser->addListener (this);
    addAndMakeVisible (*browser);

    addAndMakeVisible (closeBtn);
    closeBtn.onClick = [this] { if (onClose) onClose(); };

    addAndMakeVisible (favBox);
    favBox.setTextWhenNothingSelected ("Favourite folders...");
    favBox.onChange = [this]
    {
        const auto favs = proc.getFavourites();
        const int i = favBox.getSelectedItemIndex();
        if (i >= 0 && i < favs.size()) setRoot (File (favs[i]));
    };
    addAndMakeVisible (addFav);
    addFav.setTooltip ("Add the current folder to your favourites");
    addFav.onClick = [this]
    {
        auto favs = proc.getFavourites();
        favs.addIfNotAlreadyThere (browser->getRoot().getFullPathName());
        proc.setFavourites (favs);
        refreshFavourites();
    };
    addAndMakeVisible (removeFav);
    removeFav.setTooltip ("Remove the selected favourite");
    removeFav.onClick = [this]
    {
        auto favs = proc.getFavourites();
        favs.removeString (favBox.getText());
        proc.setFavourites (favs);
        refreshFavourites();
    };

    auto addQuick = [this] (const String& name, const File& dir)
    {
        if (! dir.isDirectory()) return;
        auto* b = quick.add (new TextButton (name));
        quickDirs.add (dir);
        b->setTooltip (dir.getFullPathName());
        b->onClick = [this, dir] { setRoot (dir); };
        addAndMakeVisible (b);
    };
    const auto home = File::getSpecialLocation (File::userHomeDirectory);
    const auto docs = File::getSpecialLocation (File::userDocumentsDirectory);
    addQuick ("HOME", home);
    addQuick ("DESKTOP", File::getSpecialLocation (File::userDesktopDirectory));
    addQuick ("DOWNLOADS", home.getChildFile ("Downloads"));
    addQuick ("MUSIC", File::getSpecialLocation (File::userMusicDirectory));
    addQuick ("DOCS", docs);
    addQuick ("ABLETON", docs.getChildFile ("Ableton").getChildFile ("User Library"));
    addQuick ("ABLETON", home.getChildFile ("Music").getChildFile ("Ableton").getChildFile ("User Library"));

    addAndMakeVisible (autoLoad);
    autoLoad.setToggleState (proc.settings().getBoolValue ("autoLoad", true), dontSendNotification);
    autoLoad.onClick = [this] { proc.settings().setValue ("autoLoad", autoLoad.getToggleState()); proc.settings().saveIfNeeded(); };

    refreshFavourites();
}

BrowserPanel::~BrowserPanel()
{
    browser->removeListener (this);
}

void BrowserPanel::refreshFavourites()
{
    favBox.clear (juce::dontSendNotification);
    int id = 1;
    for (auto& f : proc.getFavourites()) favBox.addItem (f, id++);
}

void BrowserPanel::setRoot (const juce::File& dir)
{
    if (dir.isDirectory()) browser->setRoot (dir);
}

void BrowserPanel::showFile (const juce::File& f)
{
    if (f.existsAsFile() && f.getParentDirectory() != browser->getRoot())
        browser->setRoot (f.getParentDirectory());
}

void BrowserPanel::selectionChanged()
{
    if (! autoLoad.getToggleState()) return;
    const auto f = browser->getHighlightedFile();
    if (f.existsAsFile() && f != lastLoaded && filter.isFileSuitable (f))
    {
        lastLoaded = f;
        if (onLoad) onLoad (f);
    }
}

void BrowserPanel::fileDoubleClicked (const juce::File& f)
{
    if (f.existsAsFile())
    {
        lastLoaded = f;
        if (onLoad) onLoad (f);
    }
}

void BrowserPanel::browserRootChanged (const juce::File& newRoot)
{
    proc.settings().setValue ("lastDir", newRoot.getFullPathName());
    proc.settings().saveIfNeeded();
}

void BrowserPanel::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (3, 3), 8.0f);
    drawPanel (g, getLocalBounds(), "Sample browser");
}

void BrowserPanel::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (22);
    closeBtn.setBounds (top.removeFromRight (28));
    r.removeFromTop (6);

    auto favRow = r.removeFromTop (26);
    removeFav.setBounds (favRow.removeFromRight (58));
    favRow.removeFromRight (4);
    addFav.setBounds (favRow.removeFromRight (58));
    favRow.removeFromRight (4);
    favBox.setBounds (favRow);
    r.removeFromTop (6);

    const int perRow = 3;
    for (int i = 0; i < quick.size(); i += perRow)
    {
        auto row = r.removeFromTop (24);
        const int w = row.getWidth() / perRow;
        for (int k = 0; k < perRow && i + k < quick.size(); ++k)
            quick[i + k]->setBounds (row.removeFromLeft (w).reduced (2, 0));
        r.removeFromTop (3);
    }
    r.removeFromTop (4);
    autoLoad.setBounds (r.removeFromBottom (24));
    r.removeFromBottom (4);
    browser->setBounds (r);
}
