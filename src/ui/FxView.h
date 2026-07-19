#pragma once

#include "../PluginProcessor.h"
#include "EqPanel.h"
#include "Theme.h"

// A row of chain-module cards: [EQ o] -> [COMP o] -> ... Clicking a card
// selects it (its editor shows below); clicking the power dot toggles the
// module's enable parameter (cards without one are always-on sends).
class ChainStrip : public juce::Component, private juce::Timer
{
public:
    struct Module
    {
        juce::String name;
        juce::String onParamId; // empty = no toggle (module is its own level)
    };

    ChainStrip (ChoraleProcessor&, std::function<void (int)> onSelect);
    void setModules (juce::Array<Module>);
    void setSelected (int index);
    int getSelected() const { return selected; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); } // power states follow params
    juce::Rectangle<float> cardRect (int index) const;
    bool moduleOn (int index) const;

    ChoraleProcessor& proc;
    std::function<void (int)> onSelect;
    juce::Array<Module> modules;
    int selected = 0;
};

//==============================================================================
// FX view: the selected voice's chain (EQ -> COMP -> ECHO -> VERB) and the
// master chain (EQ -> REVERB). One module's editor is shown at a time.
class FxView : public juce::Component, private juce::Timer
{
public:
    explicit FxView (ChoraleProcessor&);
    void setVoice (int v);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void initKnob (juce::Slider&, juce::Label&, const char* text);
    void showVoiceModule (int m);
    void showMasterModule (int m);

    ChoraleProcessor& proc;
    int voice = 0;

    juce::Label voiceTitle, masterTitle, voiceHint, masterHint;
    ChainStrip voiceChain, masterChain;
    EqPanel voiceEq, masterEq;
    juce::Slider compT, compR, sendEcho, sendVerb, verbSize, verbMix;
    juce::Label compTLbl, compRLbl, sendEchoLbl, sendVerbLbl, verbSizeLbl, verbMixLbl;
    juce::TextButton solo { "S" }, mute { "M" };
    std::unique_ptr<SliderAtt> compTAtt, compRAtt, sendEchoAtt, sendVerbAtt,
        verbSizeAtt, verbMixAtt;
    std::unique_ptr<ButtonAtt> soloAtt, muteAtt;
};
