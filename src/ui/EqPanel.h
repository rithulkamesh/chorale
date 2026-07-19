#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"
#include <juce_dsp/juce_dsp.h>

// Graphical 8-band EQ (band 1 = low shelf, band 8 = high shelf, rest peaks)
// with a live FFT spectrum of the bound scope channel behind the curve.
// Bound by param prefix ("v1Eq" / "mEq"); nodes are full-range draggable,
// double-click resets a band's gain.
class EqPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int kBands = 8;

    EqPanel (ChoraleProcessor&, juce::Colour accent);
    // scopeChannel: 0..7 = voices, 8 = master mix.
    void setTarget (const juce::String& prefix, int scopeChannel, juce::Colour accent);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::RangedAudioParameter* param (int band, bool gain) const;
    float bandFreq (int band) const;
    float bandGain (int band) const;
    juce::Point<float> nodePos (int band) const;
    float xToFreq (float x) const;
    float freqToX (float hz) const;
    float yToGain (float y) const;
    float gainToY (float dB) const;

    ChoraleProcessor& proc;
    juce::String prefix { "v1Eq" };
    int scopeChannel = 0;
    juce::Colour accent;
    int dragBand = -1;

    static constexpr int kFftOrder = 11, kFftSize = 1 << kFftOrder;
    juce::dsp::FFT fft { kFftOrder };
    float fftData[kFftSize * 2] {};
    float window[kFftSize] {};
    float smoothedDb[kFftSize / 2] {};
};
