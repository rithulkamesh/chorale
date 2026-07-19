#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

// Lead + 8 voice pills. Outline style; selected chip fills ink.
class NoteChips : public juce::Component
{
public:
    NoteChips (ChoraleProcessor&, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setSelected (int v) { selected = v; repaint(); }

private:
    ChoraleProcessor& processor;
    std::function<void (int)> onSelect;
    juce::Rectangle<int> chipRect (int index) const;
    int selected = 0;
};
