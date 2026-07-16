#pragma once

#include "KeyEngine.h"
#include "PsolaShifter.h"
#include "YinTracker.h"
#include <vector>

struct HarmonySettings
{
    float dryWet = 0.5f;
    int keyRoot = 0;      // pitch class, 0 = C (manual mode)
    int scaleMode = 0;    // 0 = Auto (K-S detection), 1..8 = KeyEngine scale idx + 1
    bool midiMode = false;

    struct Voice
    {
        int interval = 0; // 0 Off, 1 Oct-, 2 5th-, 3 3rd-, 4 Unison, 5 3rd+, 6 5th+, 7 Oct+
        float gain = 0.7f;
        float pan = 0.0f; // -1..1
    } voices[4];
};

// Full harmonizer chain, JUCE-free so it can run offline in tests:
// input -> YinTracker (per hop) -> KeyEngine -> per-voice ratio -> PsolaShifter
// -> gain/pan mix with latency-aligned dry.
class HarmonyEngine
{
public:
    static constexpr int kNumVoices = 4;
    static constexpr int kHop = 256;

    void prepare (double sampleRate, int maxBlockSize);
    void setSettings (const HarmonySettings& s) { settings = s; }
    void noteOn (int note);
    void noteOff (int note);
    // outL/outR must be distinct from in.
    void process (const float* in, float* outL, float* outR, int n);
    int latencySamples() const { return PsolaShifter::kLatency; }
    PitchEstimate lastPitch() const { return lastEst; }
    int detectedRootPc() const { return key.rootPc(); }
    bool detectedMinor() const { return key.isMinor(); }

private:
    void runAnalysis();

    static constexpr int kRingSize = 4096;
    static constexpr int kRingMask = kRingSize - 1;

    struct Voice
    {
        PsolaShifter shifter;
        float gainL = 0, gainR = 0;         // smoothed
        float targetGainL = 0, targetGainR = 0;
    };

    HarmonySettings settings;
    YinTracker yin;
    KeyEngine key;
    Voice voices[kNumVoices];
    PitchEstimate lastEst;

    double sr = 44100.0;
    std::vector<float> tmp;
    float anaRing[kRingSize] = {}, dryRing[kRingSize] = {};
    uint64_t anaPos = 0, dryPos = kRingSize; // dry starts one ring ahead so the delayed read is valid
    int hopRemaining = kHop;
    bool heldNotes[128] = {};
};
