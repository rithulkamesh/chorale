#pragma once

#include "dsp/HarmonyEngine.h"
#include "dsp/SignalGraph.h"
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

    // Declared before apvts so it outlives / predates the tree that uses it.
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // A/B compare: swap the whole parameter state with the stored slot.
    void toggleAB();
    std::atomic<int> abActive { 0 };

    // Editor scale, persisted with the plugin state (applied on next open).
    std::atomic<float> uiScale { 1.0f };

    // Spectrum-analyzer tap: ch 0..7 = voices, 8 = master mix.
    void readScope (int ch, float* dst, int n) const { engine.readScope (ch, dst, n); }
    // Compressor gain reduction (negative dB), same channel scheme.
    float compGrDb (int ch) const { return engine.compGrDb (ch); }

    // --- Voice-side signal graph (edges live in apvts.state -> "GRAPH") ----
    std::vector<graph::Edge> graphEdges() const;
    bool graphAddEdge (int from, int to);    // false if it would cycle
    void graphRemoveEdge (int from, int to);
    void graphResetToDefault();
    bool nodeOnCanvas (int id) const;        // wired, or explicitly placed
    juce::Point<int> nodePos (int id, juce::Point<int> fallback) const;
    void setNodePos (int id, juce::Point<int>);
    void ensureNodeVisible (int id, juce::Point<int>);
    void rebuildGraph(); // recompile from state; call after replaceState

    // Pre-1.2 states (no GRAPH child) carried wet-bus echo/reverb + per-voice
    // sends; rewrite them as a wired patch with ECHO/VERB nodes. Call on any
    // tree about to go into replaceState (host state, user preset files).
    static void migrateState (juce::ValueTree& state);

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
        *pTone, *pWidth, *pLatMode;
    juce::ValueTree abStored; // the inactive A/B slot
    std::atomic<float>*pMode[kNumVoices], *pDegree[kNumVoices], *pNote[kNumVoices],
        *pGain[kNumVoices], *pPan[kNumVoices], *pDetune[kNumVoices],
        *pSolo[kNumVoices], *pMute[kNumVoices];
    std::atomic<float>*pEqOn[kNumVoices], *pEqF[kNumVoices][8], *pEqG[kNumVoices][8],
        *pCompOn[kNumVoices], *pCompT[kNumVoices], *pCompR[kNumVoices],
        *pSatOn[kNumVoices], *pSatDrive[kNumVoices], *pSatMix[kNumVoices];
    std::atomic<float>*pMEqOn, *pMEqF[8], *pMEqG[8],
        *pMCompOn, *pMCompT, *pMCompR,
        *pMSatOn, *pMSatDrive, *pMSatMix, *pMidiAdapt, *pGainLevel[4],
        *pEchoTime[2], *pEchoFb[2], *pEchoMix[2], *pVerbSize[2], *pVerbMix[2];

    juce::ValueTree graphTree();                 // GRAPH child, created on demand
    juce::ValueTree graphTreeIfPresent() const;
    // Compiled plans stay alive until destruction: the audio thread may still
    // be reading a retired one, and plans are tiny.
    std::vector<std::unique_ptr<graph::Plan>> planKeepalive;
    std::atomic<const graph::Plan*> activePlan { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoraleProcessor)
};
