#pragma once

#include "../PluginProcessor.h"
#include "GainFader.h"
#include "Theme.h"

// Stage-view sidebar: the selected voice's mode/interval/knobs/fader + M/S.
class VoiceDetailPanel : public juce::Component
{
public:
    explicit VoiceDetailPanel (ChoraleProcessor&);
    void setVoice (int v);
    void refresh();
    void tick();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ChoraleProcessor& processor;
    int voice = 0;
    juce::Label title;
    juce::ComboBox mode, degree, note;
    juce::Slider detune, pan;
    GainFader level;
    juce::Label detuneLbl, panLbl, levelLbl;
    juce::TextButton solo { "S" }, mute { "M" };

    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ComboAtt> modeAtt, degreeAtt, noteAtt;
    std::unique_ptr<SliderAtt> detuneAtt, panAtt;
    std::unique_ptr<ButtonAtt> soloAtt, muteAtt;
};
