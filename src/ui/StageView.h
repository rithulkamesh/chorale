#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

// Radar: drag voices for pan (angle) and gain (radius). Background per the
// design system: concentric rings, staff lines, scattered dots.
class StageView : public juce::Component, private juce::Timer
{
public:
    StageView (ChoraleProcessor&, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setSelected (int v) { selected = v; repaint(); }

private:
    void timerCallback() override;
    juce::Point<float> bubblePos (int voice) const;
    void stageGeometry (float& cx, float& cy, float& rMax, float& rMin) const;
    void panGainFromPoint (juce::Point<float> pt, float& pan, float& gain) const;
    float bubbleHitRadius (int voice) const;

    ChoraleProcessor& processor;
    std::function<void (int)> onSelect;
    float smoothedLevel = 0.0f;
    float voiceLevel[ChoraleProcessor::kNumVoices] {};
    int selected = 0, dragging = -1;
};
