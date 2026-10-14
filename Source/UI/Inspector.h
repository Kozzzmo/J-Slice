#pragma once

#include "StepGrid.h"

class JSliceProcessor;

/** Everything about the selected step, written in clear : break, slice, length, re-triggers, velocity,
    pitch, reverse, per-step effects, lock. The lanes are only used to place and select hits. */
class Inspector : public juce::Component
{
public:
    explicit Inspector (JSliceProcessor& p);

    void setIndex (int index);
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    const bc::Step& step() const;
    bool hasStep() const;
    void modify (std::function<void (bc::Step&)> fn, bool undoable = true);
    juce::Rectangle<int> waveArea() const;

    JSliceProcessor& proc;
    int index = -1;
    std::vector<float> env;
    bc::Kit::Ptr envKit;
    int envSlice = -1;

    juce::TextButton srcBtn { "A" }, lockBtn { "Lock" }, prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton pitchDown { "-" }, pitchUp { "+" }, reverseBtn { "Reverse" }, clearBtn { "Delete" };
    juce::OwnedArray<juce::TextButton> rollBtns, fxBtns;
    ui::HBar lengthBar { "Length" }, velBar { "Velocity" };
};
