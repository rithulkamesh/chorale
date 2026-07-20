#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

// Small visual-feedback panels for the SAT / ECHO / VERB chain modules.

// Waveshaper transfer curve (in -1..1 -> out) with a live input-level marker.
class SatPanel : public juce::Component, private juce::Timer
{
public:
    SatPanel (ChoraleProcessor&, juce::Colour accent);
    void setTarget (const juce::String& drivePrefix, int scopeChannel, juce::Colour accent);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    ChoraleProcessor& proc;
    juce::String prefix { "v1Sat" }; // -> <prefix>Drive / <prefix>Mix
    int scopeChannel = 0;
    juce::Colour accent;
    float level = 0.0f;
    float scope[512] {};
};

// Echo node taps: repeats at the node's time, decaying by feedback,
// alternating L/R (ping-pong), breathing with the live master level.
class EchoPanel : public juce::Component, private juce::Timer
{
public:
    EchoPanel (ChoraleProcessor&, juce::Colour accent);
    void setTarget (const juce::String& paramPrefix, juce::Colour accent);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    ChoraleProcessor& proc;
    juce::String prefix { "echo1" }; // -> <prefix>Time / <prefix>Fb / <prefix>Mix
    juce::Colour accent;
    float level = 0.0f;
    float scope[512] {};
};

// Reverb node decay envelope from the node's size, scaled by its mix.
class VerbPanel : public juce::Component, private juce::Timer
{
public:
    VerbPanel (ChoraleProcessor&, juce::Colour accent);
    void setTarget (const juce::String& paramPrefix, juce::Colour accent);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    ChoraleProcessor& proc;
    juce::String prefix { "verb1" }; // -> <prefix>Size / <prefix>Mix
    juce::Colour accent;
    float level = 0.0f;
    float scope[512] {};
};
