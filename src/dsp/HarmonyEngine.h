#pragma once

#include "KeyEngine.h"
#include "PsolaShifter.h"
#include "VoiceFx.h"
#include "YinTracker.h"
#include <atomic>
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
    bool lowLatency = false; // live mode: halve PSOLA lookahead (~23 ms)

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

        // Per-voice channel chain: EQ -> compressor -> sends. Each module has
        // an explicit enable so nothing runs (or reads as loaded) by default.
        bool eqOn = false;
        float eqF[8] = { 80, 200, 500, 1200, 2500, 5000, 9000, 14000 }; // Hz
        float eqG[8] = {};               // dB; band 0 = low shelf, 7 = high shelf
        bool compOn = false;
        float compThresh = 0;            // dB
        float compRatio = 2;             // 1..8
        float sendEcho = 0, sendVerb = 0;
    } voices[8];

    // MIDI adapt: held notes retune all sounding Scale/Note voices to the
    // nearest chord tone; released -> back to their configured intervals.
    bool midiAdapt = false;

    // Master section: EQ -> compressor -> reverb bus on the main mix.
    float verbSize = 0.5f, verbMix = 0;
    bool mEqOn = false;
    float mEqF[8] = { 80, 200, 500, 1200, 2500, 5000, 9000, 14000 };
    float mEqG[8] = {};
    bool mCompOn = false;
    float mCompThresh = 0, mCompRatio = 2;
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
    void setSettings (const HarmonySettings& s)
    {
        settings = s;
        const int want = s.lowLatency ? PsolaShifter::kLiveLatency : PsolaShifter::kLatency;
        if (want != curLatency) // switching glitches briefly; the param rarely moves
        {
            curLatency = want;
            leadShifter.setLatency (want);
            for (auto& v : voices)
                v.shifter.setLatency (want);
        }
    }
    void noteOn (int note);
    void noteOff (int note);

    // Optional per-bus output taps for multi-output hosts. main is required;
    // lead and voice taps may be null and are then skipped. Lead tap carries
    // the corrected/latency-aligned lead; voice taps are post
    // gain/pan/solo/mute/humanize but pre wet-bus FX (tone/width/echo stay on
    // the main mix only). All taps share the same kLatency delay.
    struct MultiOut
    {
        float* mainL = nullptr;
        float* mainR = nullptr;
        float* leadL = nullptr;
        float* leadR = nullptr;
        float* voiceL[kNumVoices] = {};
        float* voiceR[kNumVoices] = {};
    };

    void process (const float* in, const MultiOut& out, int n);
    // Stereo-only convenience wrapper. outL/outR must be distinct from in.
    void process (const float* in, float* outL, float* outR, int n)
    {
        MultiOut out;
        out.mainL = outL;
        out.mainR = outR;
        process (in, out, n);
    }
    int latencySamples() const { return curLatency; }
    PitchEstimate lastPitch() const { return lastEst; }
    int detectedRootPc() const { return key.rootPc(); }
    bool detectedMinor() const { return key.isMinor(); }
    // Visualizer telemetry (updated per hop).
    float voiceTargetHz (int v) const { return voiceHzOut[v]; }   // 0 when not sounding
    float voiceLevel (int v) const { return voiceGainOut[v]; }
    float inputLevel() const { return lastRms; }

    // Visualiser scope taps: ch 0..7 = post-FX voices, 8 = master mix.
    // Lock-free-ish (races give at worst a glitchy frame of spectrum).
    // Compressor gain reduction (negative dB): ch 0..7 voices, 8 = master.
    float compGrDb (int ch) const { return compGrOut[ch]; }

    static constexpr int kScopeChannels = kNumVoices + 1;
    static constexpr int kScopeSize = 4096; // power of two
    void readScope (int ch, float* dst, int n) const
    {
        const uint32_t end = scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
            dst[i] = scopeRing[ch][(end - (uint32_t) n + (uint32_t) i) & (kScopeSize - 1)];
    }

private:
    void runAnalysis();

    static constexpr int kRingSize = 4096;
    static constexpr int kRingMask = kRingSize - 1;
    static constexpr int kEchoSize = 1 << 16; // ~1.3 s @ 48k
    static constexpr int kEchoMask = kEchoSize - 1;

    struct Voice
    {
        PsolaShifter shifter;
        ChannelEq eq;
        Compressor comp;
        float gainL = 0, gainR = 0; // smoothed
        float targetGainL = 0, targetGainR = 0;
        double ph1 = 0, ph2 = 0, ph3 = 0; // humanize drift oscillators
    };

    HarmonySettings settings;
    int curLatency = PsolaShifter::kLatency;
    YinTracker yin;
    KeyEngine key;
    Voice voices[kNumVoices];
    PsolaShifter leadShifter; // pitch correction path
    PitchEstimate lastEst;

    double sr = 44100.0;
    std::vector<float> tmp, leadTmp, wetL, wetR;
    std::vector<float> sendEL, sendER, sendV; // per-block send accumulators
    SimpleReverb verb;
    ChannelEq masterEqL, masterEqR;
    Compressor masterCompL, masterCompR;
    float compGrOut[kNumVoices + 1] = {};
    std::vector<float> echoL, echoR;
    uint64_t echoPos = 0;
    float toneL = 0, toneR = 0; // one-pole LPF state
    float scopeRing[kScopeChannels][kScopeSize] = {};
    std::atomic<uint32_t> scopeWrite { 0 };
    float anaRing[kRingSize] = {}, dryRing[kRingSize] = {};
    uint64_t anaPos = 0, dryPos = kRingSize; // dry starts one ring ahead so the delayed read is valid
    int hopRemaining = kHop;
    bool heldNotes[128] = {};
    float voiceHzOut[kNumVoices] = {}, voiceGainOut[kNumVoices] = {}, lastRms = 0;
};
