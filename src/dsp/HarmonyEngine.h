#pragma once

#include "KeyEngine.h"
#include "PsolaShifter.h"
#include "YinTracker.h"
#include <cstdint>
#include <vector>

struct HarmonySettings
{
    float dryWet = 0.5f;
    int keyRoot = 0;   // pitch class, 0 = C (manual mode)
    int scaleMode = 0; // 0 = Auto (K-S detection), 1..8 = KeyEngine scale idx + 1

    struct Voice
    {
        // Scale: diatonic interval from the sung note. Note: hold a fixed
        // pitch (alto-pedal style). Midi: track a held MIDI note.
        enum Mode { Off = 0, Scale = 1, Note = 2, Midi = 3 };
        int mode = Off;
        int degree = 7;      // 0..14 -> scale-step offset degree-7 (7 = unison)
        int note = 57;       // MIDI note for Note mode
        float gain = 0.7f;
        float pan = 0.0f;    // -1..1
        float detune = 0.0f; // cents
    } voices[6];
};

// Full harmonizer chain, JUCE-free so it can run offline in tests:
// input -> YinTracker (per hop) -> KeyEngine -> per-voice ratio -> PsolaShifter
// -> gain/pan mix with latency-aligned dry.
class HarmonyEngine
{
public:
    static constexpr int kNumVoices = 6;
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
    // Visualizer telemetry (updated per hop).
    float voiceTargetHz (int v) const { return voiceHzOut[v]; }   // 0 when not sounding
    float voiceLevel (int v) const { return voiceGainOut[v]; }
    float inputLevel() const { return lastRms; }

private:
    void runAnalysis();

    static constexpr int kRingSize = 4096;
    static constexpr int kRingMask = kRingSize - 1;

    struct Voice
    {
        PsolaShifter shifter;
        float gainL = 0, gainR = 0; // smoothed
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
    float voiceHzOut[kNumVoices] = {}, voiceGainOut[kNumVoices] = {}, lastRms = 0;
};
