#pragma once

#include "../PluginProcessor.h"

/** Pattern slots A01..A16 (click = edit; while playing it plays when the current pattern ends),
    COPY / PASTE / CLEAR, and the chain: CHAIN plays the list below in a loop. */
class PatternBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PatternBar (JSliceProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> slotRect (int i) const;
    juce::Rectangle<int> btnRect (int i) const;     // 0 copy, 1 paste, 2 clear
    juce::Rectangle<int> chainBtn() const;
    juce::Rectangle<int> chipRect (int i) const;
    juce::Rectangle<int> addRect() const;
    juce::Rectangle<int> clrRect() const;
    static juce::String slotName (int i) { return "A" + juce::String (i + 1).paddedLeft ('0', 2); }

    JSliceProcessor& proc;
    static constexpr int kRowH = 26, kGap = 6;
};
