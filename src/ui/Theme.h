#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Chorale design system: near-black canvas, white ink, one neutral accent,
// voices as 8 brightness steps of the same tint (no rainbow).
namespace ui
{
const juce::Colour kBg { 0xff0a0a0a };
const juce::Colour kPanel { 0xff121316 };
const juce::Colour kPanelHi { 0xff1b1d21 };
const juce::Colour kBorder { 0xff26282d };
const juce::Colour kTrack { 0xff2a2f36 }; // fader track / zero line per spec
const juce::Colour kText { 0xffffffff };
const juce::Colour kDim { 0xff8a9096 };
const juce::Colour kAccent { 0xffa6afb8 };
const juce::Colour kLead { 0xffffffff };
const juce::Colour kVoice[8] = {
    juce::Colour (0xff4a5057), juce::Colour (0xff5a616a), juce::Colour (0xff6b737d),
    juce::Colour (0xff7d8791), juce::Colour (0xff909aa4), juce::Colour (0xffa6afb8),
    juce::Colour (0xffc2c9d1), juce::Colour (0xffe2e6ea),
};

constexpr float kMinGainDb = -60.0f;
constexpr float kMaxGainDb = 6.0f;

// Embedded IBM Plex (identical rendering everywhere).
juce::Typeface::Ptr plexSans();
juce::Typeface::Ptr plexSansSemi();
juce::Typeface::Ptr plexMono();
juce::Font sans (float size, bool bold = false);
juce::Font mono (float size);

// Voice tint lifted to stay legible as text on the dark canvas; shapes keep
// the raw kVoice step.
juce::Colour voiceInk (int v);
// Ink or canvas text depending on how bright the fill behind it is.
juce::Colour textOn (juce::Colour fill);

juce::String voiceLabel (juce::AudioProcessorValueTreeState&, int voiceIndex);
float gainToDb (float gain);
float dbToGain (float db);
juce::String gainDbString (float db);
} // namespace ui
