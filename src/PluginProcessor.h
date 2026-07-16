#pragma once

#include "dsp/HarmonyEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Vocal harmonizer: mono in -> pitch detection -> key/scale interval calc ->
// N formant-preserving PSOLA voices -> pan/mix with latency-aligned dry.
// All DSP lives in src/dsp (JUCE-free, tested offline by tests/test_dsp.cpp).
class OpenHarmonyProcessor : public juce::AudioProcessor
{
public:
    OpenHarmonyProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return true; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    HarmonyEngine engine;
    std::vector<float> scratchIn, scratchR;

    std::atomic<float>*pDryWet, *pKeyRoot, *pScale, *pMidiMode;
    std::atomic<float>*pInterval[4], *pGain[4], *pPan[4];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenHarmonyProcessor)
};
