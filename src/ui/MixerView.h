#pragma once

#include "../PluginProcessor.h"
#include "GainFader.h"
#include "Theme.h"

// All eight strips at once: mode, interval, detune, pan, fader + meter.
class MixerView : public juce::Component, private juce::Timer
{
public:
    MixerView (ChoraleProcessor&, std::function<void (int)> onSelect);
    void setSelected (int v);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void refreshChannel (int v);

    struct Channel
    {
        juce::Label title;
        juce::ComboBox mode, degree, note;
        juce::Slider detune, pan;
        juce::Label detuneLbl, panLbl;
        GainFader fader;
        juce::TextButton solo { "S" }, mute { "M" };

        using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
        using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
        std::unique_ptr<ComboAtt> modeAtt, degreeAtt, noteAtt;
        std::unique_ptr<SliderAtt> detuneAtt, panAtt;
        std::unique_ptr<ButtonAtt> soloAtt, muteAtt;
    };

    ChoraleProcessor& processor;
    std::function<void (int)> onSelect;
    Channel channels[ChoraleProcessor::kNumVoices];
    int selected = 0;
};
