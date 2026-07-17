#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Chorale UI: header (preset / key / correct / mix), note-chip row, radar
// stage with draggable voice bubbles over a fine-particle field, left voice
// detail panel with solo/mute, live keyboard strip, and a global FX bar
// (humanize / tone / width / echo).
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

// Short display label for a voice's current target ("3RD^", "A3", "MIDI", "-").
juce::String voiceLabel (juce::AudioProcessorValueTreeState& apvts, int voiceIndex);
} // namespace ui

class ChoraleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChoraleLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
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
    juce::Rectangle<int> chipRect (int index) const; // 0 = lead, 1..8 = voices
    int selected = 0;
};

//==============================================================================
// Radar stage: voice bubbles (angle = pan, radius = pitch offset) over a
// fine glittering particle field. Drag a bubble horizontally for pan,
// vertically for level; click to select.
class StageView : public juce::Component, private juce::Timer
{
public:
    StageView (ChoraleProcessor&, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setSelected (int v) { selected = v; }

private:
    void timerCallback() override;
    juce::Point<float> bubblePos (int voice) const;
    float offsetSemis (int voice) const;
    void spawn (juce::Point<float> at, float intensity, juce::Colour c);

    struct Particle
    {
        float x, y, vx, vy, age, life, size, baseAlpha, twinkle;
        juce::Colour colour;
    };

    ChoraleProcessor& processor;
    std::function<void (int)> onSelect;
    std::vector<Particle> particles;
    juce::Random rng;
    float time = 0.0f, smoothedLevel = 0.0f;
    int selected = 0, dragging = -1;
    float dragStartPan = 0, dragStartGain = 0;
    juce::Point<float> dragStartPos;
};

//==============================================================================
class VoiceDetailPanel : public juce::Component
{
public:
    explicit VoiceDetailPanel (ChoraleProcessor&);
    void setVoice (int v); // rebinds all attachments to voice v's parameters
    void refresh();        // degree/note visibility follows mode
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ChoraleProcessor& processor;
    int voice = 0;
    juce::Label title;
    juce::ComboBox mode, degree, note;
    juce::Slider detune, pan, level;
    juce::Label detuneLbl, panLbl, levelLbl;
    juce::TextButton solo { "S" }, mute { "M" };

    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ComboAtt> modeAtt, degreeAtt, noteAtt;
    std::unique_ptr<SliderAtt> detuneAtt, panAtt, levelAtt;
    std::unique_ptr<ButtonAtt> soloAtt, muteAtt;
};

//==============================================================================
class KeyboardStrip : public juce::Component
{
public:
    explicit KeyboardStrip (ChoraleProcessor& p) : processor (p) {}
    void paint (juce::Graphics&) override;

private:
    ChoraleProcessor& processor;
};

//==============================================================================
class ChoraleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChoraleEditor (ChoraleProcessor&);
    ~ChoraleEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyPreset (int presetIndex);
    void selectVoice (int v);

    ChoraleProcessor& proc;
    ChoraleLookAndFeel lnf;

    juce::ComboBox preset, keyRoot, scale, correct;
    juce::Slider mix, humanize, tone, width, echoTime, echoFb, echoMix;
    juce::Label titleLbl, keyLbl, scaleLbl, correctLbl, mixLbl, autoKeyLbl, pitchLbl, latencyLbl;
    juce::Label fxLbls[6];

    NoteChips chips;
    StageView stage;
    VoiceDetailPanel detail;
    KeyboardStrip keyboard;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, correctAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        mixAtt, humanizeAtt, toneAtt, widthAtt, echoTimeAtt, echoFbAtt, echoMixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoraleEditor)
};
