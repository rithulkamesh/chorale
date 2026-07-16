#pragma once

#include "dsp/HarmonyEngine.h"

// Stock harmony shapes. Every preset is just a starting point — all voice
// parameters stay live and editable after applying one.
namespace presets
{
// Fixed-note voices in presets can't hardwire a pitch without knowing the
// song's key, so they use symbolic codes resolved against the current key
// (auto-detected or manual) at apply time, landing in alto range near A3.
enum SymbolicNote { kRoot = -1, kThird = -2, kFifth = -3 };

struct VoiceSpec
{
    int mode = 0;        // HarmonySettings::Voice::Mode
    int degree = 7;      // 0..14, 7 = unison
    int note = 57;       // MIDI note or SymbolicNote code
    float gain = 0.0f;
    float pan = 0.0f;
    float detune = 0.0f; // cents
};

struct Preset
{
    const char* category;
    const char* name;
    float dryWet;
    VoiceSpec v[6];
};

// degree helper: scale-step offset -> choice index
constexpr int D (int steps) { return steps + 7; }

constexpr int Off = 0, Scl = 1, Nte = 2, Mid = 3;

inline const Preset kPresets[] = {
    // --- Duets ---
    { "Duets", "3rd Up Duet",        0.45f, { { Scl, D(+2), 57, 0.80f, -0.20f, 0 } } },
    { "Duets", "3rd Down Duet",      0.45f, { { Scl, D(-2), 57, 0.80f,  0.20f, 0 } } },
    { "Duets", "6th Down Duet",      0.45f, { { Scl, D(-5), 57, 0.80f, -0.20f, 0 } } },
    { "Duets", "5th Up Air",         0.40f, { { Scl, D(+4), 57, 0.55f,  0.30f, 0 } } },
    { "Duets", "Country Duet",       0.45f, { { Scl, D(+2), 57, 0.75f, -0.25f, 0 },
                                              { Scl, D(+7), 57, 0.30f,  0.35f, 0 } } },
    // --- Stacks ---
    { "Stacks", "Pop Stack",         0.45f, { { Scl, D(+2), 57, 0.80f, -0.40f, 0 },
                                              { Scl, D(+4), 57, 0.60f,  0.40f, 0 } } },
    { "Stacks", "Pop Wide 3rds",     0.45f, { { Scl, D(+2), 57, 0.70f, -0.70f, -10 },
                                              { Scl, D(+2), 57, 0.70f,  0.70f, +10 } } },
    { "Stacks", "R&B Close",         0.42f, { { Scl, D(+2), 57, 0.60f, -0.15f, 0 },
                                              { Scl, D(+4), 57, 0.45f,  0.15f, 0 } } },
    { "Stacks", "R&B 6ths",          0.42f, { { Scl, D(-5), 57, 0.65f, -0.25f, 0 },
                                              { Scl, D(+2), 57, 0.50f,  0.25f, 0 } } },
    { "Stacks", "Soul Warmth",       0.45f, { { Scl, D(-2), 57, 0.70f, -0.30f, 0 },
                                              { Scl, D(-4), 57, 0.50f,  0.30f, 0 } } },
    { "Stacks", "Beach Stack",       0.45f, { { Scl, D(+3), 57, 0.60f, -0.30f, 0 },
                                              { Scl, D(+5), 57, 0.55f,  0.30f, 0 } } },
    { "Stacks", "Quartal (Jazz)",    0.42f, { { Scl, D(+3), 57, 0.60f, -0.30f, 0 },
                                              { Scl, D(+6), 57, 0.50f,  0.30f, 0 } } },
    // --- Choirs ---
    { "Choirs", "Gospel Triad Up",   0.45f, { { Scl, D(+2), 57, 0.70f, -0.40f, 0 },
                                              { Scl, D(+4), 57, 0.60f,  0.40f, 0 },
                                              { Scl, D(+7), 57, 0.40f,  0.00f, 0 } } },
    { "Choirs", "Gospel Choir",      0.48f, { { Scl, D(-2), 57, 0.55f, -0.60f, 0 },
                                              { Scl, D(+2), 57, 0.65f,  0.60f, 0 },
                                              { Scl, D(+4), 57, 0.55f, -0.30f, 0 },
                                              { Scl, D(+7), 57, 0.35f,  0.30f, 0 },
                                              { Scl, D(-7), 57, 0.45f,  0.00f, 0 } } },
    { "Choirs", "Full Choir",        0.50f, { { Scl, D(-7), 57, 0.50f,  0.00f, 0 },
                                              { Scl, D(-4), 57, 0.45f, -0.55f, 0 },
                                              { Scl, D(+2), 57, 0.60f, -0.80f, -8 },
                                              { Scl, D(+2), 57, 0.60f,  0.80f, +8 },
                                              { Scl, D(+4), 57, 0.50f,  0.55f, 0 },
                                              { Scl, D(+7), 57, 0.30f,  0.00f, 0 } } },
    { "Choirs", "Trap Choir",        0.50f, { { Scl, D(-7), 57, 0.80f,  0.00f, 0 },
                                              { Scl, D(+2), 57, 0.40f, -0.70f, 0 },
                                              { Scl, D(+7), 57, 0.35f,  0.70f, 0 } } },
    // --- Octaves ---
    { "Octaves", "Octave Up",        0.42f, { { Scl, D(+7), 57, 0.70f,  0.00f, 0 } } },
    { "Octaves", "Octave Down",      0.42f, { { Scl, D(-7), 57, 0.80f,  0.00f, 0 } } },
    { "Octaves", "Octaves Both",     0.45f, { { Scl, D(+7), 57, 0.55f, -0.30f, 0 },
                                              { Scl, D(-7), 57, 0.65f,  0.30f, 0 } } },
    { "Octaves", "Power 5ths",       0.45f, { { Scl, D(+4), 57, 0.65f, -0.25f, 0 },
                                              { Scl, D(-7), 57, 0.60f,  0.25f, 0 } } },
    { "Octaves", "Stadium",          0.48f, { { Scl, D(+4), 57, 0.55f, -0.50f, 0 },
                                              { Scl, D(+7), 57, 0.50f,  0.50f, 0 },
                                              { Scl, D(-7), 57, 0.60f,  0.00f, 0 } } },
    { "Octaves", "Hip-Hop Low",      0.45f, { { Scl, D(-7), 57, 0.90f,  0.00f, 0 },
                                              { Scl, D( 0), 57, 0.45f, -0.60f, -12 },
                                              { Scl, D( 0), 57, 0.45f,  0.60f, +12 } } },
    // --- Doublers ---
    { "Doublers", "Tight Double",    0.40f, { { Scl, D(0), 57, 0.70f, -0.50f, -9 },
                                              { Scl, D(0), 57, 0.70f,  0.50f, +9 } } },
    { "Doublers", "Thick Double x4", 0.45f, { { Scl, D(0), 57, 0.60f, -0.80f, -14 },
                                              { Scl, D(0), 57, 0.60f,  0.80f, +14 },
                                              { Scl, D(0), 57, 0.55f, -0.35f, +7 },
                                              { Scl, D(0), 57, 0.55f,  0.35f, -7 } } },
    { "Doublers", "Double + Air",    0.42f, { { Scl, D(0),  57, 0.60f, -0.50f, -10 },
                                              { Scl, D(0),  57, 0.60f,  0.50f, +10 },
                                              { Scl, D(+7), 57, 0.30f,  0.00f, 0 } } },
    // --- Pedals (constant-note voices) ---
    { "Pedals", "Alto Pedal (Root)", 0.42f, { { Nte, D(0), kRoot,  0.70f, 0.15f, 0 } } },
    { "Pedals", "Alto Pedal (5th)",  0.42f, { { Nte, D(0), kFifth, 0.65f, -0.15f, 0 } } },
    { "Pedals", "Root + 5th Drone",  0.45f, { { Nte, D(0), kRoot,  0.60f, -0.35f, 0 },
                                              { Nte, D(0), kFifth, 0.55f,  0.35f, 0 } } },
    { "Pedals", "Static Chord Pad",  0.48f, { { Nte, D(0), kRoot,  0.55f, -0.50f, 0 },
                                              { Nte, D(0), kThird, 0.50f,  0.00f, 0 },
                                              { Nte, D(0), kFifth, 0.50f,  0.50f, 0 } } },
    { "Pedals", "Pedal + 3rd Up",    0.45f, { { Nte, D(0),  kRoot, 0.60f, -0.30f, 0 },
                                              { Scl, D(+2), 57,    0.60f,  0.30f, 0 } } },
    // --- MIDI ---
    { "MIDI", "MIDI Chord",          0.48f, { { Mid, D(0), 57, 0.65f, -0.60f, 0 },
                                              { Mid, D(0), 57, 0.65f,  0.00f, 0 },
                                              { Mid, D(0), 57, 0.65f,  0.60f, 0 },
                                              { Mid, D(0), 57, 0.55f, -0.25f, 0 } } },
    { "MIDI", "MIDI + Octave Down",  0.48f, { { Mid, D(0),  57, 0.65f, -0.45f, 0 },
                                              { Mid, D(0),  57, 0.65f,  0.45f, 0 },
                                              { Scl, D(-7), 57, 0.55f,  0.00f, 0 } } },
    // --- Experimental ---
    { "Experimental", "2nds Cluster", 0.38f, { { Scl, D(+1), 57, 0.45f, -0.40f, 0 },
                                               { Scl, D(-1), 57, 0.45f,  0.40f, 0 } } },
};

inline constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));

// Resolve a symbolic note code against a key, near alto A3 (MIDI 57).
inline int resolveNote (int code, int rootPc, bool minorKey)
{
    if (code >= 0)
        return code;
    int pc = rootPc;
    if (code == kThird)
        pc = (rootPc + (minorKey ? 3 : 4)) % 12;
    else if (code == kFifth)
        pc = (rootPc + 7) % 12;
    int note = pc + 48; // C3 octave
    if (note < 51)      // keep within ~D#3..D#4 alto-ish band
        note += 12;
    return note;
}
} // namespace presets
