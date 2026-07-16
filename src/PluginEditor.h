#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Dark custom UI: header (preset browser, key/scale, mix), six voice strips
// (mode / interval-or-note / detune / pan / gain), footer with live detected
// pitch and key.
namespace ui
{
const juce::Colour kBg { 0xff101216 };
const juce::Colour kPanel { 0xff181c23 };
const juce::Colour kPanelHi { 0xff232833 };
const juce::Colour kText { 0xffe8eaf0 };
const juce::Colour kDim { 0xff8a90a0 };
const juce::Colour kVoice[6] = {
    juce::Colour (0xff5aa9ff), juce::Colour (0xff7ee08a), juce::Colour (0xffffc95c),
    juce::Colour (0xffff8a7a), juce::Colour (0xffc79bff), juce::Colour (0xff5fd4d0),
};
} // namespace ui

class OpenHarmonyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OpenHarmonyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

// ChromaVerb-style particle field: fine audio-reactive particles on a dark
// panel. Particle x-position follows pitch (log scale), colour follows the
// voice it belongs to, spawn rate follows input level and voice gain.
class Visualizer : public juce::Component, private juce::Timer
{
public:
    explicit Visualizer (OpenHarmonyProcessor&);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void spawn (float hz, float intensity, juce::Colour c);

    struct Particle
    {
        float x, y, vx, vy, age, life, size, baseAlpha;
        juce::Colour colour;
    };

    OpenHarmonyProcessor& processor;
    std::vector<Particle> particles;
    juce::Random rng;
    float time = 0.0f, smoothedLevel = 0.0f;
};

class VoiceStrip : public juce::Component
{
public:
    VoiceStrip (juce::AudioProcessorValueTreeState& apvts, int voiceIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh(); // sync degree/note visibility with mode

private:
    juce::AudioProcessorValueTreeState& apvts;
    const int index;
    juce::ComboBox mode, degree, note;
    juce::Slider detune, pan, gain;
    juce::Label title, detuneLbl, panLbl;

    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<ComboAtt> modeAtt, degreeAtt, noteAtt;
    std::unique_ptr<SliderAtt> detuneAtt, panAtt, gainAtt;
};

class OpenHarmonyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OpenHarmonyEditor (OpenHarmonyProcessor&);
    ~OpenHarmonyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyPreset (int presetIndex);

    OpenHarmonyProcessor& processor;
    OpenHarmonyLookAndFeel lnf;

    juce::ComboBox preset, keyRoot, scale;
    juce::Slider mix;
    juce::Label titleLbl, keyLbl, scaleLbl, mixLbl, autoKeyLbl, pitchLbl, latencyLbl;
    Visualizer visualizer;
    juce::OwnedArray<VoiceStrip> strips;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenHarmonyEditor)
};
