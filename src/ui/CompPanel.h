#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

// Compressor transfer-curve graph: in/out dB plot with a draggable threshold
// node on the knee, a live input-level dot riding the curve, and a gain-
// reduction meter on the right edge. Bound by param prefix ("v1Comp"/"mComp"
// -> <prefix>T / <prefix>R params).
class CompPanel : public juce::Component, private juce::Timer
{
public:
    CompPanel (ChoraleProcessor&, juce::Colour accent);
    // scopeChannel: 0..7 = voices, 8 = master (for level dot + GR meter).
    void setTarget (const juce::String& prefix, int scopeChannel, juce::Colour accent);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::RangedAudioParameter* param (bool ratio) const;
    float thresh() const;
    float ratio() const;
    float outDbFor (float inDb) const;
    juce::Rectangle<float> plotArea() const;
    float dbToX (float dB) const;
    float dbToY (float dB) const;

    ChoraleProcessor& proc;
    juce::String prefix { "v1Comp" };
    int scopeChannel = 0;
    juce::Colour accent;
    bool dragging = false;
    float levelDb = -60.0f, grSmooth = 0.0f;
    float scope[512] {};
};
