#pragma once

#include "KeyEngine.h"
#include "PsolaShifter.h"
#include "YinTracker.h"
#include <cstdint>
#include <vector>

struct HarmonySettings
{
    float dryWet = 0.5f;
    int keyRoot = 0;      // pitch class, 0 = C (manual mode)
    int scaleMode = 0;    // 0 = Auto (K-S detection), 1..8 = KeyEngine scale idx + 1
    int correct = 0;      // lead pitch correction: 0 off, 1 natural (partial), 2 hard
    float humanize = 0;   // 0..1 slow pitch/level drift on harmony voices
    float tone = 20000;   // wet-bus lowpass cutoff Hz
    float width = 1.0f;   // wet-bus stereo width, 0 mono .. 2 extra wide
    float echoTime = 0;   // wet-bus echo delay ms (0 disables)
    float echoFb = 0.35f; // echo feedback 0..0.9
    float echoMix = 0;    // echo send level 0..1

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
        bool solo = false;
        bool mute = false;
    } voices[8];
};

// Full harmonizer chain, JUCE-free so it can run offline in tests:
// input -> YinTracker (per hop) -> KeyEngine -> per-voice ratio ->
// N x PsolaShifter -> pan/solo/mute mix -> wet-bus FX (tone/width/echo) ->
// blend with lead (delayed dry, or pitch-corrected via its own shifter).
class HarmonyEngine
{
public:
    static constexpr int kNumVoices = 8;
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
    static constexpr int kEchoSize = 1 << 16; // ~1.3 s @ 48k
    static constexpr int kEchoMask = kEchoSize - 1;

    struct Voice
    {
        PsolaShifter shifter;
        float gainL = 0, gainR = 0; // smoothed
        float targetGainL = 0, targetGainR = 0;
        double ph1 = 0, ph2 = 0, ph3 = 0; // humanize drift oscillators
    };

    HarmonySettings settings;
    YinTracker yin;
    KeyEngine key;
    Voice voices[kNumVoices];
    PsolaShifter leadShifter; // pitch correction path
    PitchEstimate lastEst;

    double sr = 44100.0;
    std::vector<float> tmp, leadTmp, wetL, wetR;
    std::vector<float> echoL, echoR;
    uint64_t echoPos = 0;
    float toneL = 0, toneR = 0; // one-pole LPF state
    float anaRing[kRingSize] = {}, dryRing[kRingSize] = {};
    uint64_t anaPos = 0, dryPos = kRingSize; // dry starts one ring ahead so the delayed read is valid
    int hopRemaining = kHop;
    bool heldNotes[128] = {};
    float voiceHzOut[kNumVoices] = {}, voiceGainOut[kNumVoices] = {}, lastRms = 0;
};
