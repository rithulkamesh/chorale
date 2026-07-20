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

// Nearest note (in semitones) whose pitch class is in the held-MIDI set;
// ties resolve downward.
int snapToHeldPc (int note, const bool heldPc[12])
{
    for (int d = 0; d <= 6; ++d)
    {
        if (heldPc[((note - d) % 12 + 12) % 12])
            return note - d;
        if (heldPc[((note + d) % 12 + 12) % 12])
            return note + d;
    }
    return note;
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
    for (int n = 0; n < graph::kNumNodes; ++n)
    {
        nodeL[n].assign (chunk, 0.0f);
        nodeR[n].assign (chunk, 0.0f);
    }
    for (int v = 0; v < kNumVoices; ++v)
    {
        nodeEqL[v].reset();
        nodeEqR[v].reset();
        nodeCompL[v].prepare (sr);
        nodeCompR[v].prepare (sr);
    }
    defaultPlan = graph::compile (graph::defaultEdges());
    masterEqL.reset();
    masterEqR.reset();
    masterCompL.prepare (sr);
    masterCompR.prepare (sr);
    for (int e = 0; e < 2; ++e)
    {
        echoRingL[e].assign (kEchoSize, 0.0f);
        echoRingR[e].assign (kEchoSize, 0.0f);
        echoPos[e] = kEchoSize * 4;
        nodeVerb[e].prepare (sr);
    }
    toneL = toneR = 0;
    std::fill (std::begin (anaRing), std::end (anaRing), 0.0f);
    std::fill (std::begin (dryRing), std::end (dryRing), 0.0f);
    anaPos = 0;
    dryPos = kRingSize;
    hopRemaining = kHop;
    lastEst = {};
    std::fill (std::begin (heldNotes), std::end (heldNotes), false);
    updateVoiceGate();
    leadActive = settings.correct != 0;
}

void HarmonyEngine::updateVoiceGate (const bool midiSounding[kNumVoices])
{
    using VM = HarmonySettings::Voice;
    bool anySolo = false;
    for (const auto& vs : settings.voices)
        anySolo = anySolo || (vs.solo && vs.mode != VM::Off);

    int numHeld = 0;
    for (int note = 0; note < 128; ++note)
        if (heldNotes[note])
            ++numHeld;

    int midiIdx = 0;
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        const auto& vs = settings.voices[vi];
        voiceActive[vi] = vs.mode != VM::Off && ! vs.mute && ! (anySolo && ! vs.solo);
        if (vs.mode == VM::Midi)
        {
            if (midiSounding != nullptr)
                voiceActive[vi] = voiceActive[vi] && midiSounding[vi];
            else
                voiceActive[vi] = voiceActive[vi] && midiIdx < numHeld;
            if (voiceActive[vi])
                ++midiIdx;
        }
    }
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
        if (leadActive)
            leadShifter.process (src, leadTmp.data(), todo);
        for (int i = 0; i < todo; ++i)
        {
            anaRing[anaPos++ & kRingMask] = src[i];
            dryRing[dryPos & kRingMask] = src[i];
            if (! leadActive)
                leadTmp[(size_t) i] = dryRing[(dryPos - (uint64_t) curLatency) & kRingMask];
            else if (settings.correct == 0)
                leadTmp[(size_t) i] = dryRing[(dryPos - (uint64_t) curLatency) & kRingMask];
            ++dryPos;
        }

        if (out.leadL != nullptr && out.leadR != nullptr)
            for (int i = 0; i < todo; ++i)
            {
                out.leadL[done + i] = leadTmp[(size_t) i];
                out.leadR[done + i] = leadTmp[(size_t) i];
            }

        // 1. Sources: shifted, humanized, levelled, panned voice signals.
        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            auto& v = voices[vi];
            float* sL = nodeL[graph::kVoice0 + vi].data();
            float* sR = nodeR[graph::kVoice0 + vi].data();
            if (voiceActive[vi])
            {
                v.shifter.process (src, tmp.data(), todo);
                for (int i = 0; i < todo; ++i)
                {
                    v.gainL += 0.002f * (v.targetGainL - v.gainL);
                    v.gainR += 0.002f * (v.targetGainR - v.gainR);
                    sL[i] = v.gainL * tmp[(size_t) i];
                    sR[i] = v.gainR * tmp[(size_t) i];
                }
            }
            else
            {
                for (int i = 0; i < todo; ++i)
                {
                    v.gainL += 0.002f * (v.targetGainL - v.gainL);
                    v.gainR += 0.002f * (v.targetGainR - v.gainR);
                    sL[i] = 0.0f;
                    sR[i] = 0.0f;
                }
            }
        }

        // 2. Run the signal graph: sum inputs, process, per node in topo order.
        const graph::Plan* activePlan = plan != nullptr && plan->valid ? plan : &defaultPlan;
        for (int n : activePlan->order)
        {
            float* dL = nodeL[n].data();
            float* dR = nodeR[n].data();
            const auto& ins = activePlan->inputs[n];
            {
                const float* aL = nodeL[ins[0]].data();
                const float* aR = nodeR[ins[0]].data();
                std::copy_n (aL, todo, dL);
                std::copy_n (aR, todo, dR);
                for (size_t k = 1; k < ins.size(); ++k)
                {
                    const float* bL = nodeL[ins[k]].data();
                    const float* bR = nodeR[ins[k]].data();
                    for (int i = 0; i < todo; ++i)
                    {
                        dL[i] += bL[i];
                        dR[i] += bR[i];
                    }
                }
            }

            const int slot = n % kNumVoices; // pool index within its kind
            switch (graph::kindOf (n))
            {
                case graph::NodeKind::Eq:
                {
                    const auto& vs = settings.voices[slot];
                    if (vs.eqOn && nodeEqL[slot].active)
                        for (int i = 0; i < todo; ++i)
                        {
                            dL[i] = nodeEqL[slot].process (dL[i]);
                            dR[i] = nodeEqR[slot].process (dR[i]);
                        }
                    break;
                }
                case graph::NodeKind::Comp:
                {
                    const auto& vs = settings.voices[slot];
                    if (vs.compOn && vs.compThresh < -0.5f)
                        for (int i = 0; i < todo; ++i)
                        {
                            dL[i] = nodeCompL[slot].process (dL[i], vs.compThresh, vs.compRatio);
                            dR[i] = nodeCompR[slot].process (dR[i], vs.compThresh, vs.compRatio);
                        }
                    break;
                }
                case graph::NodeKind::Sat:
                {
                    const auto& vs = settings.voices[slot];
                    if (vs.satOn && vs.satMix > 0.001f)
                        for (int i = 0; i < todo; ++i)
                        {
                            dL[i] = Saturator::process (dL[i], vs.satDrive, vs.satMix);
                            dR[i] = Saturator::process (dR[i], vs.satDrive, vs.satMix);
                        }
                    break;
                }
                case graph::NodeKind::Gain:
                {
                    const float g = settings.gainLevel[n - graph::kGain0];
                    for (int i = 0; i < todo; ++i)
                    {
                        dL[i] *= g;
                        dR[i] *= g;
                    }
                    break;
                }
                case graph::NodeKind::Echo:
                {
                    const int e = n - graph::kEcho0;
                    const auto& es = settings.echo[e];
                    const int d = (int) (std::clamp (es.time, 0.0f, 1200.0f) * 0.001f * sr);
                    if (d > 32 && es.mix > 0.001f)
                    {
                        float* rL = echoRingL[e].data();
                        float* rR = echoRingR[e].data();
                        uint64_t& pos = echoPos[e];
                        const float fb = std::clamp (es.fb, 0.0f, 0.9f);
                        for (int i = 0; i < todo; ++i)
                        {
                            const float el = rL[(pos - (uint64_t) d) & kEchoMask];
                            const float er = rR[(pos - (uint64_t) d) & kEchoMask];
                            rL[pos & kEchoMask] = dL[i] + fb * er; // ping-pong
                            rR[pos & kEchoMask] = dR[i] + fb * el;
                            ++pos;
                            dL[i] += es.mix * el;
                            dR[i] += es.mix * er;
                        }
                    }
                    break;
                }
                case graph::NodeKind::Verb:
                {
                    const int e = n - graph::kVerb0;
                    const auto& vb = settings.verb[e];
                    if (vb.mix > 0.001f)
                        for (int i = 0; i < todo; ++i)
                        {
                            float rl, rr;
                            nodeVerb[e].process ((dL[i] + dR[i]) * 0.5f * vb.mix, rl, rr);
                            dL[i] += rl;
                            dR[i] += rr;
                        }
                    break;
                }
                case graph::NodeKind::Out:
                case graph::NodeKind::Voice:
                    break; // OUT: summed input *is* the wet bus
            }
        }

        // 3. Wet bus = OUT node input sum (silence when nothing reaches OUT).
        if (! activePlan->inputs[graph::kOut].empty())
        {
            std::copy_n (nodeL[graph::kOut].data(), todo, wetL.data());
            std::copy_n (nodeR[graph::kOut].data(), todo, wetR.data());
        }
        else
        {
            std::fill_n (wetL.begin(), (size_t) todo, 0.0f);
            std::fill_n (wetR.begin(), (size_t) todo, 0.0f);
        }

        // 4. Stems and scopes tap the end of each voice's lane (post-FX,
        // matching the old fixed chains).
        for (int vi = 0; vi < kNumVoices; ++vi)
        {
            const int tap = activePlan->stemTap[vi];
            const float* tL = nodeL[tap].data();
            const float* tR = nodeR[tap].data();
            float* stemL = out.voiceL[vi];
            float* stemR = out.voiceR[vi];
            for (int i = 0; i < todo; ++i)
            {
                scopeRing[vi][(scopeWrite.load (std::memory_order_relaxed) + (uint32_t) i)
                              & (kScopeSize - 1)] = tL[i] + tR[i];
                if (stemL != nullptr)
                {
                    stemL[done + i] = tL[i];
                    stemR[done + i] = tR[i];
                }
            }
        }

        // Wet-bus FX: one-pole tone LPF -> mid/side width. Echo and reverb
        // live on the canvas as graph nodes now.
        const float toneCoef =
            1.0f - std::exp (-2.0f * kPi * std::clamp (settings.tone, 200.0f, 20000.0f) / (float) sr);
        const float width = std::clamp (settings.width, 0.0f, 2.0f);
        const float dryGain = 1.0f - settings.dryWet;
        const bool masterEqOn = settings.mEqOn && masterEqL.active;
        const bool masterCompOn = settings.mCompOn && settings.mCompThresh < -0.5f;
        const bool masterSatOn = settings.mSatOn && settings.mSatMix > 0.001f;

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

            const float lead = leadTmp[(size_t) i];
            L[i] = dryGain * lead + settings.dryWet * wl;
            R[i] = dryGain * lead + settings.dryWet * wr;
            if (masterEqOn)
            {
                L[i] = masterEqL.process (L[i]);
                R[i] = masterEqR.process (R[i]);
            }
            if (masterCompOn)
            {
                L[i] = masterCompL.process (L[i], settings.mCompThresh, settings.mCompRatio);
                R[i] = masterCompR.process (R[i], settings.mCompThresh, settings.mCompRatio);
            }
            if (masterSatOn)
            {
                L[i] = Saturator::process (L[i], settings.mSatDrive, settings.mSatMix);
                R[i] = Saturator::process (R[i], settings.mSatDrive, settings.mSatMix);
            }
            scopeRing[kNumVoices][(scopeWrite.load (std::memory_order_relaxed) + (uint32_t) i)
                                  & (kScopeSize - 1)] = (L[i] + R[i]) * 0.5f;
        }
        scopeWrite.fetch_add ((uint32_t) todo, std::memory_order_relaxed);

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
    leadActive = settings.correct != 0;
    if (leadActive)
    {
        leadShifter.setPeriod (lastEst.voiced ? (float) (sr / lastEst.f0) : 0.0f, lastEst.voiced);
        leadShifter.setRatio (corrRatio);
    }

    // Channel-strip coefficients follow the settings at hop rate (cheap, and
    // avoids threading games with the UI).
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        const auto& vs = settings.voices[vi];
        nodeEqL[vi].set ((float) sr, vs.eqF, vs.eqG);
        nodeEqR[vi].set ((float) sr, vs.eqF, vs.eqG);
    }
    masterEqL.set ((float) sr, settings.mEqF, settings.mEqG);
    masterEqR.set ((float) sr, settings.mEqF, settings.mEqG);
    for (int e = 0; e < 2; ++e)
        nodeVerb[e].setSize (settings.verb[e].size);

    int held[16], numHeld = 0;
    bool heldPc[12] = {};
    for (int note = 0; note < 128; ++note)
        if (heldNotes[note])
        {
            if (numHeld < 16)
                held[numHeld++] = note;
            heldPc[note % 12] = true;
        }
    const bool adapt = settings.midiAdapt && numHeld > 0;

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
                    int target = scaleIdx == KeyEngine::kChromatic
                                     ? nearest + chromaticSemis (steps)
                                     : KeyEngine::harmonyTarget (nearest, rootPc, scaleIdx, steps);
                    if (adapt) // held chord overrides the diatonic target
                        target = snapToHeldPc (target, heldPc);
                    // Offset from the *nearest* note so the singer's detune
                    // carries into the harmony instead of being auto-tuned away.
                    ratio = std::exp2 ((float) (target - nearest) / 12.0f);
                    sounding = true;
                    break;
                }
                case VM::Note: // absolute pitch: alto-pedal / drone voice
                {
                    int target = vs.note;
                    if (adapt)
                        target = snapToHeldPc (target, heldPc);
                    ratio = (float) std::exp2 ((target - detMidi) / 12.0);
                    sounding = true;
                    break;
                }
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

        // Unvoiced passthrough stays audible (doubles consonants/breaths);
        // a MIDI voice with no assigned note goes silent.
        float g = vs.mode != VM::Off ? vs.gain * levelMul : 0.0f;
        if (vs.mode == VM::Midi && ! sounding)
            g = 0.0f;
        if (vs.mute || (anySolo && ! vs.solo))
            g = 0.0f;

        voiceActive[vi] = vs.mode != VM::Off && ! (vs.mute || (anySolo && ! vs.solo))
                          && ! (vs.mode == VM::Midi && ! sounding);

        v.shifter.setPeriod (lastEst.voiced ? (float) (sr / lastEst.f0) : 0.0f, lastEst.voiced);
        v.shifter.setRatio (ratio);

        const float p = (std::clamp (vs.pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi; // constant power
        v.targetGainL = g * std::cos (p);
        v.targetGainR = g * std::sin (p);

        voiceHzOut[vi] = sounding && g > 0.0f ? lastEst.f0 * ratio : 0.0f;
        voiceGainOut[vi] = sounding ? g : 0.0f;
        compGrOut[vi] = vs.compOn ? std::min (nodeCompL[vi].grDb, nodeCompR[vi].grDb) : 0.0f;
    }
    compGrOut[kNumVoices] = settings.mCompOn
                                ? std::min (masterCompL.grDb, masterCompR.grDb)
                                : 0.0f;
}
