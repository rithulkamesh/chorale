#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

// The patch canvas: node cards wired with bezier cables. Drag a card to move
// it, drag from its right-edge port to another card to wire (inputs sum),
// right-click a cable to cut it, right-click empty canvas to add nodes or
// reset, click a card to open its editor (host view decides what that means).
class GraphCanvas : public juce::Component, private juce::Timer
{
public:
    GraphCanvas (ChoraleProcessor&, std::function<void (int)> onSelect);
    void setSelected (int nodeId);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }

    static juce::String nodeName (int id);
    static juce::String powerParam (int id); // empty = no toggle
    bool nodeVisible (int id) const;
    juce::Rectangle<float> nodeRect (int id) const;
    juce::Point<float> outPort (int id) const;
    juce::Point<float> inPort (int id) const;
    juce::Point<float> inPortFor (int id, float sourceY) const;
    juce::Path cablePath (juce::Point<float> a, juce::Point<float> b) const;
    int nodeAt (juce::Point<float>) const;
    bool cableAt (juce::Point<float>, graph::Edge& hit) const;
    juce::Point<int> defaultPos (int id) const;
    void showAddMenu (juce::Point<int> canvasPos);

    ChoraleProcessor& proc;
    std::function<void (int)> onSelect;
    int selected = 0;
    int dragNode = -1, cableFrom = -1;
    juce::Point<float> dragOffset, mousePos;
};
