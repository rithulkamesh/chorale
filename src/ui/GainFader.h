#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

// Vertical dB fader with a thin-line meter and a right-click type-in dialog.
class GainFader : public juce::Component, private juce::Timer
{
public:
    GainFader();
    void bind (ChoraleProcessor&, const juce::String& paramId);
    void setMeterLevel (float level);
    float getSmoothedMeter() const { return smoothedMeter; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void syncFromParam();
    void pushToParam();
    void updateLabel();

    ChoraleProcessor* processor = nullptr;
    juce::String paramId;
    float meterLevel = 0.0f, smoothedMeter = 0.0f;
    juce::Slider slider;
    juce::Label valueLbl;
};
