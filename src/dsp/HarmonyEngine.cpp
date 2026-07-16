#include "HarmonyEngine.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kPi = 3.14159265358979f;

// Scale-step offset -> semitones through a major pattern; used only for the
// chromatic "scale", where degrees have no diatonic meaning.
int chromaticSemis (int steps)
{
    static const int maj[7] = { 0, 2, 4, 5, 7, 9, 11 };
    int k = steps >= 0 ? steps / 7 : -((-steps + 6) / 7);
    const int i = steps - 7 * k;
    return 12 * k + maj[i];
}
} // namespace

void HarmonyEngine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    yin.prepare (sr);
    key.reset();
    for (auto& v : voices)
    {
        v.shifter.prepare (sr);
        v.gainL = v.gainR = v.targetGainL = v.targetGainR = 0;
    }
    tmp.assign ((size_t) std::max (maxBlockSize, kHop), 0.0f);
    std::fill (std::begin (anaRing), std::end (anaRing), 0.0f);
    std::fill (std::begin (dryRing), std::end (dryRing), 0.0f);
    anaPos = 0;
    dryPos = kRingSize;
    hopRemaining = kHop;
    lastEst = {};
    std::fill (std::begin (heldNotes), std::end (heldNotes), false);
}

void HarmonyEngine::noteOn (int note)
{
    if (note >= 0 && note < 128)
        heldNotes[note] = true;
}

void HarmonyEngine::noteOff (int note)
{
    if (note >= 0 && note < 128)
        heldNotes[note] = false;
}

void HarmonyEngine::process (const float* in, float* outL, float* outR, int n)
{
    int done = 0;
    while (done < n)
    {
        const int todo = std::min (n - done, hopRemaining);
        const float* src = in + done;
        float* L = outL + done;
        float* R = outR + done;

        // Latency-aligned dry so it lines up with the shifted voices.
        const float dryGain = 1.0f - settings.dryWet;
        for (int i = 0; i < todo; ++i)
        {
            anaRing[anaPos++ & kRingMask] = src[i];
            dryRing[dryPos & kRingMask] = src[i];
            const float dry = dryRing[(dryPos - (uint64_t) PsolaShifter::kLatency) & kRingMask];
            ++dryPos;
            L[i] = dryGain * dry;
            R[i] = dryGain * dry;
        }

        for (auto& v : voices)
        {
            v.shifter.process (src, tmp.data(), todo);
            for (int i = 0; i < todo; ++i)
            {
                v.gainL += 0.002f * (v.targetGainL - v.gainL);
                v.gainR += 0.002f * (v.targetGainR - v.gainR);
                L[i] += settings.dryWet * v.gainL * tmp[(size_t) i];
                R[i] += settings.dryWet * v.gainR * tmp[(size_t) i];
            }
        }

        done += todo;
        hopRemaining -= todo;
        if (hopRemaining == 0)
        {
            runAnalysis();
            hopRemaining = kHop;
        }
    }
}

void HarmonyEngine::runAnalysis()
{
    float frame[YinTracker::kFrame];
    for (int i = 0; i < YinTracker::kFrame; ++i)
        frame[i] = anaRing[(anaPos - (uint64_t) YinTracker::kFrame + (uint64_t) i) & kRingMask];

    lastEst = yin.analyze (frame);
    if (lastEst.voiced)
        key.addObservation (lastEst.f0, lastEst.confidence);

    double hopEnergy = 0.0;
    for (int i = YinTracker::kFrame - kHop; i < YinTracker::kFrame; ++i)
        hopEnergy += frame[i] * frame[i];
    lastRms = (float) std::sqrt (hopEnergy / kHop);

    int rootPc, scaleIdx;
    if (settings.scaleMode == 0)
    {
        rootPc = key.rootPc();
        scaleIdx = key.isMinor() ? 1 : 0;
    }
    else
    {
        rootPc = settings.keyRoot;
        scaleIdx = settings.scaleMode - 1;
    }

    const double detMidi = lastEst.voiced ? 69.0 + 12.0 * std::log2 (lastEst.f0 / 440.0) : 0.0;
    const int nearest = (int) std::lround (detMidi);

    int held[16], numHeld = 0;
    for (int note = 0; note < 128 && numHeld < 16; ++note)
        if (heldNotes[note])
            held[numHeld++] = note;

    int midiIdx = 0;
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[vi];
        const auto& vs = settings.voices[vi];
        using VM = HarmonySettings::Voice;

        float ratio = 1.0f;
        bool sounding = false;
        if (vs.mode != VM::Off && lastEst.voiced)
        {
            switch (vs.mode)
            {
                case VM::Scale:
                {
                    const int steps = vs.degree - 7;
                    const int target = scaleIdx == KeyEngine::kChromatic
                                           ? nearest + chromaticSemis (steps)
                                           : KeyEngine::harmonyTarget (nearest, rootPc, scaleIdx, steps);
                    // Offset from the *nearest* note so the singer's detune
                    // carries into the harmony instead of being auto-tuned away.
                    ratio = std::exp2 ((float) (target - nearest) / 12.0f);
                    sounding = true;
                    break;
                }
                case VM::Note: // absolute pitch: alto-pedal / drone voice
                    ratio = (float) std::exp2 ((vs.note - detMidi) / 12.0);
                    sounding = true;
                    break;
                case VM::Midi:
                    if (midiIdx < numHeld)
                    {
                        ratio = (float) std::exp2 ((held[midiIdx++] - detMidi) / 12.0);
                        sounding = true;
                    }
                    break;
                default: break;
            }
        }
        ratio *= std::exp2 (vs.detune / 1200.0f);

        v.shifter.setPeriod (lastEst.voiced ? (float) (sr / lastEst.f0) : 0.0f, lastEst.voiced);
        v.shifter.setRatio (ratio);

        // Unvoiced passthrough stays audible (doubles consonants/breaths);
        // a MIDI voice with no assigned note goes silent.
        float g = vs.mode != VM::Off ? vs.gain : 0.0f;
        if (vs.mode == VM::Midi && ! sounding)
            g = 0.0f;

        const float p = (std::clamp (vs.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi; // constant power
        v.targetGainL = g * std::cos (p);
        v.targetGainR = g * std::sin (p);

        voiceHzOut[vi] = sounding ? lastEst.f0 * ratio : 0.0f;
        voiceGainOut[vi] = sounding ? g : 0.0f;
    }
}
