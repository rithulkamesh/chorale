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
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[vi];
        v.shifter.prepare (sr);
        v.gainL = v.gainR = v.targetGainL = v.targetGainR = 0;
        // Deterministic per-voice drift rates/phases so humanize is stable
        // across runs but decorrelated between voices.
        v.ph1 = 0.61 * vi;
        v.ph2 = 1.17 * vi + 0.35;
        v.ph3 = 0.83 * vi + 0.11;
    }
    leadShifter.prepare (sr);
    const size_t chunk = (size_t) std::max (maxBlockSize, kHop);
    tmp.assign (chunk, 0.0f);
    leadTmp.assign (chunk, 0.0f);
    wetL.assign (chunk, 0.0f);
    wetR.assign (chunk, 0.0f);
    echoL.assign (kEchoSize, 0.0f);
    echoR.assign (kEchoSize, 0.0f);
    echoPos = kEchoSize * 4;
    toneL = toneR = 0;
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

void HarmonyEngine::process (const float* in, const MultiOut& out, int n)
{
    int done = 0;
    while (done < n)
    {
        const int todo = std::min (n - done, hopRemaining);
        const float* src = in + done;
        float* L = out.mainL + done;
        float* R = out.mainR + done;

        // Lead path: pitch-corrected (own shifter) or latency-aligned dry.
        // The correction shifter always runs so its buffers stay warm when
        // toggling; both paths share the same kLatency delay.
        leadShifter.process (src, leadTmp.data(), todo);
        for (int i = 0; i < todo; ++i)
        {
            anaRing[anaPos++ & kRingMask] = src[i];
            dryRing[dryPos & kRingMask] = src[i];
            if (settings.correct == 0)
                leadTmp[(size_t) i] = dryRing[(dryPos - (uint64_t) PsolaShifter::kLatency) & kRingMask];
            ++dryPos;
        }

        if (out.leadL != nullptr && out.leadR != nullptr)
            for (int i = 0; i < todo; ++i)
            {
                out.leadL[done + i] = leadTmp[(size_t) i];
                out.leadR[done + i] = leadTmp[(size_t) i];
            }

        std::fill_n (wetL.begin(), (size_t) todo, 0.0f);
        std::fill_n (wetR.begin(), (size_t) todo, 0.0f);
        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            auto& v = voices[vi];
            float* tapL = out.voiceL[vi];
            float* tapR = out.voiceR[vi];
            v.shifter.process (src, tmp.data(), todo);
            for (int i = 0; i < todo; ++i)
            {
                v.gainL += 0.002f * (v.targetGainL - v.gainL);
                v.gainR += 0.002f * (v.targetGainR - v.gainR);
                const float vl = v.gainL * tmp[(size_t) i];
                const float vr = v.gainR * tmp[(size_t) i];
                wetL[(size_t) i] += vl;
                wetR[(size_t) i] += vr;
                if (tapL != nullptr)
                {
                    tapL[done + i] = vl;
                    tapR[done + i] = vr;
                }
            }
        }

        // Wet-bus FX: one-pole tone LPF -> mid/side width -> echo.
        const float toneCoef =
            1.0f - std::exp (-2.0f * kPi * std::clamp (settings.tone, 200.0f, 20000.0f) / (float) sr);
        const float width = std::clamp (settings.width, 0.0f, 2.0f);
        const int echoSamps = (int) (std::clamp (settings.echoTime, 0.0f, 1200.0f) * 0.001f * sr);
        const bool echoOn = echoSamps > 32 && settings.echoMix > 0.001f;
        const float fb = std::clamp (settings.echoFb, 0.0f, 0.9f);
        const float dryGain = 1.0f - settings.dryWet;

        for (int i = 0; i < todo; ++i)
        {
            float wl = wetL[(size_t) i], wr = wetR[(size_t) i];

            toneL += toneCoef * (wl - toneL);
            toneR += toneCoef * (wr - toneR);
            wl = toneL;
            wr = toneR;

            const float mid = 0.5f * (wl + wr);
            const float side = 0.5f * (wl - wr) * width;
            wl = mid + side;
            wr = mid - side;

            if (echoOn)
            {
                const float el = echoL[(echoPos - (uint64_t) echoSamps) & kEchoMask];
                const float er = echoR[(echoPos - (uint64_t) echoSamps) & kEchoMask];
                echoL[echoPos & kEchoMask] = wl + fb * er; // ping-pong cross-feedback
                echoR[echoPos & kEchoMask] = wr + fb * el;
                ++echoPos;
                wl += settings.echoMix * el;
                wr += settings.echoMix * er;
            }

            const float lead = leadTmp[(size_t) i];
            L[i] = dryGain * lead + settings.dryWet * wl;
            R[i] = dryGain * lead + settings.dryWet * wr;
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
    const double hopDt = kHop / sr;

    // Lead pitch correction: snap toward the nearest scale note.
    float corrRatio = 1.0f;
    if (settings.correct != 0 && lastEst.voiced)
    {
        const int snapped = KeyEngine::harmonyTarget (nearest, rootPc,
                                                      scaleIdx == KeyEngine::kChromatic ? 0 : scaleIdx, 0);
        const float strength = settings.correct == 2 ? 1.0f : 0.65f;
        corrRatio = (float) std::exp2 (strength * (snapped - detMidi) / 12.0);
    }
    leadShifter.setPeriod (lastEst.voiced ? (float) (sr / lastEst.f0) : 0.0f, lastEst.voiced);
    leadShifter.setRatio (corrRatio);

    int held[16], numHeld = 0;
    for (int note = 0; note < 128 && numHeld < 16; ++note)
        if (heldNotes[note])
            held[numHeld++] = note;

    bool anySolo = false;
    for (const auto& vs : settings.voices)
        anySolo = anySolo || (vs.solo && vs.mode != HarmonySettings::Voice::Off);

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

        // Humanize: slow decorrelated pitch drift + level flutter, like real
        // singers hovering around the note.
        float driftCents = vs.detune;
        float levelMul = 1.0f;
        if (settings.humanize > 0.001f)
        {
            v.ph1 += 2.0 * kPi * (0.31 + 0.07 * vi) * hopDt;
            v.ph2 += 2.0 * kPi * (0.83 + 0.13 * vi) * hopDt;
            v.ph3 += 2.0 * kPi * (0.19 + 0.05 * vi) * hopDt;
            driftCents += settings.humanize * (9.0f * (float) std::sin (v.ph1)
                                               + 4.0f * (float) std::sin (v.ph2));
            levelMul = 1.0f + settings.humanize * 0.15f * (float) std::sin (v.ph3);
        }
        ratio *= std::exp2 (driftCents / 1200.0f);

        v.shifter.setPeriod (lastEst.voiced ? (float) (sr / lastEst.f0) : 0.0f, lastEst.voiced);
        v.shifter.setRatio (ratio);

        // Unvoiced passthrough stays audible (doubles consonants/breaths);
        // a MIDI voice with no assigned note goes silent.
        float g = vs.mode != VM::Off ? vs.gain * levelMul : 0.0f;
        if (vs.mode == VM::Midi && ! sounding)
            g = 0.0f;
        if (vs.mute || (anySolo && ! vs.solo))
            g = 0.0f;

        const float p = (std::clamp (vs.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi; // constant power
        v.targetGainL = g * std::cos (p);
        v.targetGainR = g * std::sin (p);

        voiceHzOut[vi] = sounding && g > 0.0f ? lastEst.f0 * ratio : 0.0f;
        voiceGainOut[vi] = sounding ? g : 0.0f;
    }
}
