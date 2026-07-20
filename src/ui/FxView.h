#pragma once

#include "../PluginProcessor.h"
#include "CompPanel.h"
#include "EqPanel.h"
#include "FxPanels.h"
#include "GraphCanvas.h"
#include "Theme.h"

// A row of chain-module cards (used by the fixed master chain).
class ChainStrip : public juce::Component, private juce::Timer
{
public:
    struct Module
    {
        juce::String name;
        juce::String onParamId; // empty = no toggle
    };

    ChainStrip (ChoraleProcessor&, std::function<void (int)> onSelect);
    void setModules (juce::Array<Module>);
    void setSelected (int index);
    int getSelected() const { return selected; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override { repaint(); }
    juce::Rectangle<float> cardRect (int index) const;
    bool moduleOn (int index) const;

    ChoraleProcessor& proc;
    std::function<void (int)> onSelect;
    juce::Array<Module> modules;
    int selected = 0;
};

//==============================================================================
// FX view: the patchable voice-side signal graph on a canvas, the selected
// node's editor below it, and the fixed master chain (EQ -> COMP -> SAT) on
// the right.
class FxView : public juce::Component, private juce::Timer
{
public:
    explicit FxView (ChoraleProcessor&);
    void setVoice (int v); // selects that voice's source node
    std::function<void (int)> onVoicePicked; // canvas voice click -> host sync

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void initKnob (juce::Slider&, juce::Label&, const char* text);
    void selectNode (int nodeId);
    void showMasterModule (int m);

    ChoraleProcessor& proc;
    int selectedNode = 0;

    juce::Label nodeTitle, masterTitle, nodeHint, masterHint;
    GraphCanvas canvas;
    ChainStrip masterChain;

    // Node editors (one visible at a time, bound to the selected node).
    EqPanel eqPanel;
    CompPanel compPanel;
    SatPanel satPanel;
    EchoPanel echoPanel;
    VerbPanel verbPanel;
    juce::Slider compT, compR, satDrive, satMix, gainKnob,
        echoTime, echoFb, echoMix, verbSize, verbMix;
    juce::Label compTLbl, compRLbl, satDriveLbl, satMixLbl, gainLbl,
        echoTimeLbl, echoFbLbl, echoMixLbl, verbSizeLbl, verbMixLbl;
    std::unique_ptr<SliderAtt> compTAtt, compRAtt, satDriveAtt, satMixAtt, gainAtt,
        echoTimeAtt, echoFbAtt, echoMixAtt, verbSizeAtt, verbMixAtt;
    juce::TextButton solo { "S" }, mute { "M" };
    std::unique_ptr<ButtonAtt> soloAtt, muteAtt;

    // Master editors.
    EqPanel masterEq;
    CompPanel masterComp;
    SatPanel masterSat;
    juce::Slider mCompT, mCompR, mSatDrive, mSatMix;
    juce::Label mCompTLbl, mCompRLbl, mSatDriveLbl, mSatMixLbl;
    std::unique_ptr<SliderAtt> mCompTAtt, mCompRAtt, mSatDriveAtt, mSatMixAtt;
};
