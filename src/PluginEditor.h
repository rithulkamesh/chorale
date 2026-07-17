#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{
const juce::Colour kBg { 0xff0e1014 };
const juce::Colour kPanel { 0xff161a21 };
const juce::Colour kPanelHi { 0xff222735 };
const juce::Colour kText { 0xffe9ebf2 };
const juce::Colour kDim { 0xff868da0 };
const juce::Colour kLead { 0xffff9d8a };
const juce::Colour kVoice[8] = {
    juce::Colour (0xff5aa9ff), juce::Colour (0xff7ee08a), juce::Colour (0xffffc95c),
    juce::Colour (0xffff8a7a), juce::Colour (0xffc79bff), juce::Colour (0xff5fd4d0),
    juce::Colour (0xfff08ed8), juce::Colour (0xffb5e06a),
};

juce::String voiceLabel (juce::AudioProcessorValueTreeState& apvts, int voiceIndex);
float gainToDb (float gain);
float dbToGain (float db);
juce::String gainDbString (float db);
} // namespace ui

class ChoraleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChoraleLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                           float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
};

//==============================================================================
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

//==============================================================================
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

//==============================================================================
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

//==============================================================================
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

        using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
        using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
        std::unique_ptr<ComboAtt> modeAtt, degreeAtt, noteAtt;
        std::unique_ptr<SliderAtt> detuneAtt, panAtt;
    };

    ChoraleProcessor& processor;
    std::function<void (int)> onSelect;
    Channel channels[ChoraleProcessor::kNumVoices];
    int selected = 0;
};

//==============================================================================
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

//==============================================================================
class ChoraleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChoraleEditor (ChoraleProcessor&);
    ~ChoraleEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyPreset (int presetIndex);
    void selectVoice (int v);
    void setMainView (bool mixer);
    void layoutHeader();

    ChoraleProcessor& proc;
    ChoraleLookAndFeel lnf;

    juce::ComboBox preset, keyRoot, scale, correct;
    juce::Slider mix, humanize, tone, width, echoTime, echoFb, echoMix;
    juce::Label titleLbl, keyLbl, scaleLbl, correctLbl, mixLbl, autoKeyLbl, pitchLbl, latencyLbl;
    juce::Label fxLbls[6];
    juce::TextButton stageBtn { "STAGE" }, mixerBtn { "MIXER" };

    NoteChips chips;
    StageView stage;
    MixerView mixer;
    VoiceDetailPanel detail;

    bool showMixer = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, correctAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        mixAtt, humanizeAtt, toneAtt, widthAtt, echoTimeAtt, echoFbAtt, echoMixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoraleEditor)
};
