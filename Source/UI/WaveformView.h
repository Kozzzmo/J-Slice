#pragma once

#include "LookAndFeel.h"

class JSliceProcessor;

/** Display of one break (A or B) : waveform, beat grid and the slice markers (editable). */
class WaveformView : public juce::Component, public juce::SettableTooltipClient
{
public:
    WaveformView (JSliceProcessor& p, int slot);

    std::function<void()> onActivate;   // clicked : this break becomes the one edited by the slicer row
    std::function<void()> onEmptyClick; // clicked while empty : open a file
    bool dimmed = false;                // break B switched off

    void refresh();                 // kit changed
    void tick();                    // ~30 Hz playback feedback

    void paint (juce::Graphics&) override;
    void resized() override         { rebuildPeaks(); }
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> waveArea() const;
    float sampleToX (double s) const;
    double xToSample (float x) const;
    int markerAt (float x) const;
    int tagAt (juce::Point<float> p) const;          // slice whose tag (top strip) is under the mouse
    juce::Rectangle<float> tagRect (int slice, bool& full) const;
    int sliceAt (float x) const;
    void rebuildPeaks();
    void showSliceMenu (int slice);

    JSliceProcessor& proc;
    const int slot;
    bc::Kit::Ptr kit;
    std::vector<std::pair<float, float>> peaks;
    int hoverMarker = -1, dragMarker = -1, selected = -1, playingSlice = -1;
    double playPos = -1.0;
    bool markerMoved = false;
    std::vector<bc::Slice> dragSlices;
};
