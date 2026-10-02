#pragma once

#include "LookAndFeel.h"

class JSliceProcessor;

/** Green LCD showing the break, the beat grid and the slice markers (editable). */
class WaveformView : public juce::Component
{
public:
    explicit WaveformView (JSliceProcessor& p);

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
    int sliceAt (float x) const;
    void rebuildPeaks();
    void showSliceMenu (int slice);

    JSliceProcessor& proc;
    bc::Kit::Ptr kit;
    std::vector<std::pair<float, float>> peaks;
    int hoverMarker = -1, dragMarker = -1, selected = -1, playingSlice = -1;
    double playPos = -1.0;
    std::vector<bc::Slice> dragSlices;
};
