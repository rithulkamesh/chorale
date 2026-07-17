#pragma once

#include "dsp/HarmonyEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>

// Chorale: vocal harmonizer / stacker. mono in -> pitch detection -> key/scale
// interval calc -> N formant-preserving PSOLA voices (+ lead pitch correction)
// -> wet-bus FX -> mix with latency-aligned lead.
// All DSP lives in src/dsp (JUCE-free, tested offline by tests/test_dsp.cpp).
class ChoraleProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kNumVoices = HarmonyEngine::kNumVoices;

    ChoraleProcessor();

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
    double getTailLengthSeconds() const override           { return 2.0; } // echo tail

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Live telemetry for the editor (written on the audio thread).
    std::atomic<float> uiF0 { 0.0f };   // 0 = unvoiced
    std::atomic<int> uiRoot { 0 };
    std::atomic<bool> uiMinor { false };
    std::atomic<float> uiLevel { 0.0f };
    std::atomic<float> uiVoiceHz[HarmonyEngine::kNumVoices] {};
    std::atomic<float> uiVoiceGain[HarmonyEngine::kNumVoices] {};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static BusesProperties choraleBuses();

    HarmonyEngine engine;
    std::vector<float> scratchIn, scratchR;

    std::atomic<float>*pDryWet, *pKeyRoot, *pScale, *pCorrect, *pHumanize,
        *pTone, *pWidth, *pEchoTime, *pEchoFb, *pEchoMix;
    std::atomic<float>*pMode[kNumVoices], *pDegree[kNumVoices], *pNote[kNumVoices],
        *pGain[kNumVoices], *pPan[kNumVoices], *pDetune[kNumVoices],
        *pSolo[kNumVoices], *pMute[kNumVoices];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoraleProcessor)
};
