// Offline DSP tests: synthesized vocal-like signals, closed-loop verification
// (harmonized output is pitch-tracked and asserted against the target
// interval), plus demo WAV renders in tests_out/ for listening.
#include "dsp/HarmonyEngine.h"
#include "dsp/KeyEngine.h"
#include "dsp/PsolaShifter.h"
#include "dsp/YinTracker.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
constexpr double kSr = 44100.0;
constexpr double kPi = 3.14159265358979323846;
int failures = 0;

#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        if (! (cond))                                                        \
        {                                                                    \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

#define CHECK_NEAR(a, b, relTol)                                             \
    do                                                                       \
    {                                                                        \
        const double va = (a), vb = (b);                                     \
        if (std::abs (va - vb) > (relTol) * std::abs (vb))                   \
        {                                                                    \
            std::printf ("FAIL %s:%d  %s=%g expected %g (+-%g%%)\n",         \
                         __FILE__, __LINE__, #a, va, vb, 100.0 * (relTol));  \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

double midiHz (double m) { return 440.0 * std::exp2 ((m - 69.0) / 12.0); }

// Band-limited vocal-ish tone: 20 harmonics at 1/k, slight 5 Hz vibrato.
std::vector<float> synth (const std::vector<std::pair<double, double>>& notes, // {midi, seconds}
                          double vibratoDepth = 0.005)
{
    std::vector<float> out;
    double phase = 0.0, t = 0.0;
    for (const auto& [midi, dur] : notes)
    {
        const int n = (int) (dur * kSr);
        const double f = midiHz (midi);
        for (int i = 0; i < n; ++i)
        {
            const double f0 = f * (1.0 + vibratoDepth * std::sin (2.0 * kPi * 5.0 * t));
            phase += f0 / kSr;
            double s = 0.0;
            for (int k = 1; k <= 20; ++k)
                if (k * f0 < kSr / 2.0)
                    s += std::sin (2.0 * kPi * k * phase) / k;
            out.push_back ((float) (0.25 * s));
            t += 1.0 / kSr;
        }
    }
    return out;
}

// Median voiced f0 over a sample range.
double trackedF0 (const std::vector<float>& x, int start, int end)
{
    YinTracker yin;
    yin.prepare (kSr);
    std::vector<double> f0s;
    for (int i = start; i + YinTracker::kFrame <= std::min (end, (int) x.size()); i += 512)
    {
        const auto est = yin.analyze (x.data() + i);
        if (est.voiced)
            f0s.push_back (est.f0);
    }
    if (f0s.empty())
        return 0.0;
    std::sort (f0s.begin(), f0s.end());
    return f0s[f0s.size() / 2];
}

void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r = {})
{
    const int ch = r.empty() ? 1 : 2;
    const uint32_t frames = (uint32_t) l.size();
    const uint32_t dataBytes = frames * (uint32_t) ch * 2;
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); u32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (1); u16 ((uint16_t) ch);
    u32 ((uint32_t) kSr); u32 ((uint32_t) kSr * (uint32_t) ch * 2); u16 ((uint16_t) (ch * 2)); u16 (16);
    f.write ("data", 4); u32 (dataBytes);
    for (uint32_t i = 0; i < frames; ++i)
        for (int c = 0; c < ch; ++c)
        {
            const float s = std::clamp (c == 0 ? l[i] : r[i], -1.0f, 1.0f);
            u16 ((uint16_t) (int16_t) std::lround (s * 32767.0f));
        }
}

struct EngineResult { std::vector<float> l, r; };

// Tests run against the classic full chains so per-voice FX are in-lane.
const graph::Plan& chainPlan()
{
    static const graph::Plan p = graph::compile (graph::chainEdges());
    return p;
}

EngineResult runEngine (const std::vector<float>& in, const HarmonySettings& s,
                        const std::vector<int>& midiNotes = {},
                        const graph::Plan* plan = nullptr)
{
    HarmonyEngine eng;
    eng.prepare (kSr, 512);
    eng.setGraph (plan != nullptr ? plan : &chainPlan());
    eng.setSettings (s);
    for (int note : midiNotes)
        eng.noteOn (note);
    EngineResult out;
    out.l.resize (in.size());
    out.r.resize (in.size());
    for (size_t i = 0; i < in.size(); i += 512)
    {
        const int n = (int) std::min<size_t> (512, in.size() - i);
        eng.process (in.data() + i, out.l.data() + i, out.r.data() + i, n);
    }
    return out;
}
} // namespace

int main()
{
    std::filesystem::create_directories ("tests_out");

    // 1. YIN accuracy on steady + vibrato tones.
    {
        const auto tone = synth ({ { 57, 1.0 } }); // A3 = 220 Hz
        CHECK_NEAR (trackedF0 (tone, 4096, (int) tone.size()), 220.0, 0.015);
        const auto high = synth ({ { 69, 1.0 } }); // A4 = 440 Hz
        CHECK_NEAR (trackedF0 (high, 4096, (int) high.size()), 440.0, 0.015);
        std::puts ("ok: YIN tracks 220/440 Hz within 1.5%");
    }

    // 2. Diatonic harmony targets.
    {
        // C major (root 0, scale 0): 3rds/5ths/octave from C4=60.
        CHECK (KeyEngine::harmonyTarget (60, 0, 0, 2) == 64);  // C4 -> E4
        CHECK (KeyEngine::harmonyTarget (60, 0, 0, 4) == 67);  // C4 -> G4
        CHECK (KeyEngine::harmonyTarget (60, 0, 0, 7) == 72);  // C4 -> C5
        CHECK (KeyEngine::harmonyTarget (60, 0, 0, -2) == 57); // C4 -> A3
        CHECK (KeyEngine::harmonyTarget (62, 0, 0, 2) == 65);  // D4 -> F4 (minor 3rd, diatonic)
        CHECK (KeyEngine::harmonyTarget (71, 0, 0, 2) == 74);  // B3.. B4 -> D5
        // A natural minor (root 9, scale 1): A3 -> C4.
        CHECK (KeyEngine::harmonyTarget (57, 9, 1, 2) == 60);
        // Chromatic passing note snaps to nearest degree first: C#4 in C major, +2 deg.
        const int t = KeyEngine::harmonyTarget (61, 0, 0, 2);
        CHECK (t == 64 || t == 65); // from C or D, both legal nearest degrees
        std::puts ("ok: diatonic interval calculator");
    }

    // 3. Krumhansl-Schmuckler auto key detection.
    {
        KeyEngine key;
        key.reset();
        // C major melody, tonic-weighted.
        const int melody[] = { 60, 64, 67, 60, 62, 64, 65, 67, 69, 67, 64, 60, 67, 60 };
        for (int pass = 0; pass < 8; ++pass)
            for (int m : melody)
                key.addObservation ((float) midiHz (m), 1.0f);
        CHECK (key.rootPc() == 0);
        CHECK (! key.isMinor());
        std::puts ("ok: K-S detects C major from melody");
    }

    // 4. PSOLA closed loop: shift A3 up a fifth -> E4.
    {
        const auto tone = synth ({ { 57, 1.5 } }, 0.0);
        PsolaShifter sh;
        sh.prepare (kSr);
        std::vector<float> out (tone.size());
        const float ratio = (float) std::exp2 (7.0 / 12.0);
        for (size_t i = 0; i < tone.size(); i += 512)
        {
            const int n = (int) std::min<size_t> (512, tone.size() - i);
            sh.setPeriod ((float) (kSr / 220.0), true);
            sh.setRatio (ratio);
            sh.process (tone.data() + i, out.data() + i, n);
        }
        CHECK_NEAR (trackedF0 (out, 8192, (int) out.size()), 220.0 * ratio, 0.03);
        writeWav ("tests_out/psola_fifth_up.wav", out);
        std::puts ("ok: PSOLA fifth-up lands within 3%");
    }

    // 5. Engine end-to-end, Scale mode: C4 sung, voice 1 = 3rd Up -> E4.
    {
        const auto tone = synth ({ { 60, 1.2 } });
        HarmonySettings s;
        s.dryWet = 1.0f; // wet only
        s.scaleMode = 1; // manual C major
        s.keyRoot = 0;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f }; // Scale, 3rd Up
        const auto out = runEngine (tone, s);
        CHECK_NEAR (trackedF0 (out.l, 16384, (int) out.l.size()), midiHz (64), 0.03);
        std::puts ("ok: engine Scale mode 3rd-up C4->E4 within 3%");
    }

    // 6. Engine end-to-end, MIDI mode: A3 sung, held E4 -> harmony at E4.
    {
        const auto tone = synth ({ { 57, 1.2 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.voices[0] = { 3, 7, 57, 1.0f, 0.0f, 0.0f }; // MIDI mode
        const auto out = runEngine (tone, s, { 64 });
        CHECK_NEAR (trackedF0 (out.l, 16384, (int) out.l.size()), midiHz (64), 0.03);
        std::puts ("ok: engine MIDI mode locks harmony to held E4 within 3%");
    }

    // 6b. Note mode (alto pedal): melody moves C4->E4, voice holds A3 throughout.
    {
        const auto tone = synth ({ { 60, 0.7 }, { 64, 0.7 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.voices[0] = { 2, 7, 57, 1.0f, 0.0f, 0.0f }; // Note mode, A3
        const auto out = runEngine (tone, s);
        CHECK_NEAR (trackedF0 (out.l, 16384, (int) (0.6 * kSr)), 220.0, 0.03);
        CHECK_NEAR (trackedF0 (out.l, (int) (0.85 * kSr), (int) out.l.size()), 220.0, 0.03);
        std::puts ("ok: engine Note mode holds A3 pedal across melody change");
    }

    // 6c. Solo isolates a voice: v1 (3rd up) soloed, v2 (octave up) active but
    // not soloed -> output pitch is v1's E4, not polyphonic mush.
    {
        const auto tone = synth ({ { 60, 1.2 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.scaleMode = 1;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f, true, false };  // Scale 3rd up, SOLO
        s.voices[1] = { 1, 14, 57, 1.0f, 0.0f, 0.0f, false, false }; // Scale oct up
        const auto out = runEngine (tone, s);
        CHECK_NEAR (trackedF0 (out.l, 16384, (int) out.l.size()), midiHz (64), 0.03);
        std::puts ("ok: solo isolates one voice");
    }

    // 6d. Hard pitch correction: lead sung 40 cents sharp of C4 -> corrected
    // output lands on C4.
    {
        const auto tone = synth ({ { 60.4, 1.2 } });
        HarmonySettings s;
        s.dryWet = 0.0f; // lead only
        s.scaleMode = 1;
        s.correct = 2;
        const auto out = runEngine (tone, s);
        const double f = trackedF0 (out.l, 16384, (int) out.l.size());
        CHECK_NEAR (f, midiHz (60), 0.012); // within ~20 cents of C4
        std::puts ("ok: hard correction snaps +40c lead to the scale note");
    }

    // 6e. Multi-output stems: voice buses carry isolated post-fader voices,
    // the lead bus carries the lead, and the main bus is identical to the
    // stereo-only wrapper for the same deterministic settings.
    {
        const auto tone = synth ({ { 60, 1.2 } });
        HarmonySettings s;
        s.dryWet = 0.5f;
        s.scaleMode = 1; // manual C major, humanize/echo off -> deterministic
        s.humanize = 0.0f;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f };  // 3rd up
        s.voices[1] = { 1, 11, 57, 1.0f, 0.0f, 0.0f }; // 5th up

        const size_t len = tone.size();
        std::vector<float> mainL (len), mainR (len), leadL (len), leadR (len),
            v1L (len), v1R (len), v2L (len), v2R (len);

        HarmonyEngine eng;
        eng.prepare (kSr, 512);
        eng.setSettings (s);
        for (size_t i = 0; i < len; i += 512)
        {
            const int n = (int) std::min<size_t> (512, len - i);
            HarmonyEngine::MultiOut mo;
            mo.mainL = mainL.data() + i;
            mo.mainR = mainR.data() + i;
            mo.leadL = leadL.data() + i;
            mo.leadR = leadR.data() + i;
            mo.voiceL[0] = v1L.data() + i;
            mo.voiceR[0] = v1R.data() + i;
            mo.voiceL[1] = v2L.data() + i;
            mo.voiceR[1] = v2R.data() + i;
            eng.process (tone.data() + i, mo, n);
        }

        CHECK_NEAR (trackedF0 (v1L, 16384, (int) len), midiHz (64), 0.03);  // E4 stem
        CHECK_NEAR (trackedF0 (v2L, 16384, (int) len), midiHz (67), 0.03);  // G4 stem
        CHECK_NEAR (trackedF0 (leadL, 16384, (int) len), midiHz (60), 0.03); // lead stem

        const auto ref = runEngine (tone, s); // stereo wrapper, same settings
        double maxDiff = 0.0;
        for (size_t i = 0; i < len; ++i)
            maxDiff = std::max (maxDiff, (double) std::abs (ref.l[i] - mainL[i]));
        CHECK (maxDiff < 1e-6);
        std::puts ("ok: multi-out stems isolated; main bus matches stereo mix");
    }

    // 6f. Per-voice channel FX: mid-band cut at the harmony fundamental drops
    // level, the compressor changes gain, and a wired REVERB node rings past
    // the end of the input.
    {
        auto tone = synth ({ { 60, 1.0 } });
        tone.resize (tone.size() + (size_t) (0.5 * kSr), 0.0f); // silence for tails
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.scaleMode = 1;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f }; // 3rd up (E4 ~ 330 Hz)

        auto rms = [] (const std::vector<float>& x, size_t a, size_t b)
        {
            double e = 0;
            for (size_t i = a; i < b; ++i)
                e += x[i] * x[i];
            return std::sqrt (e / (double) (b - a));
        };
        const size_t n = tone.size();
        const auto flat = runEngine (tone, s);
        const double flatBody = rms (flat.l, n / 4, n / 2);
        const double flatTail = rms (flat.l, n - (size_t) (0.2 * kSr), n);

        auto cutS = s;
        cutS.voices[0].eqOn = true;
        cutS.voices[0].eqF[3] = 330.0f; // peak band on the harmony fundamental
        cutS.voices[0].eqG[3] = -12.0f;
        const auto cut = runEngine (tone, cutS);
        CHECK (rms (cut.l, n / 4, n / 2) < flatBody * 0.7);

        auto compS = s;
        compS.voices[0].compOn = true;
        compS.voices[0].compThresh = -30.0f;
        compS.voices[0].compRatio = 8.0f;
        const auto comp = runEngine (tone, compS);
        const double dbDelta = 20.0 * std::log10 (rms (comp.l, n / 4, n / 2) / flatBody);
        CHECK (std::abs (dbDelta) > 1.0); // gain computer clearly engaged

        // Reverb as a graph node: splice VERB0 into voice 1's lane end.
        auto verbEdges = graph::chainEdges();
        verbEdges.erase (std::remove (verbEdges.begin(), verbEdges.end(),
                                      graph::Edge { graph::kSat0, graph::kOut }),
                         verbEdges.end());
        verbEdges.push_back ({ graph::kSat0, graph::kVerb0 });
        verbEdges.push_back ({ graph::kVerb0, graph::kOut });
        const auto verbPlan = graph::compile (verbEdges);
        CHECK (verbPlan.valid);
        auto verbS = s;
        verbS.verb[0].size = 0.8f;
        verbS.verb[0].mix = 1.0f;
        const auto verb = runEngine (tone, verbS, {}, &verbPlan);
        CHECK (rms (verb.l, n - (size_t) (0.2 * kSr), n) > flatTail * 4.0 + 1e-4);
        std::puts ("ok: per-voice EQ cut, compressor gain, reverb-node tail");
    }

    // 6g. Master EQ: -12 dB high shelf on the mix darkens the output.
    {
        const auto tone = synth ({ { 60, 1.2 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.scaleMode = 1;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f };
        const auto flat = runEngine (tone, s);
        auto dark = s;
        dark.mEqOn = true;
        dark.mEqG[7] = -12.0f; // high shelf
        dark.mEqF[7] = 2000.0f;
        const auto out = runEngine (tone, dark);
        auto hf = [] (const std::vector<float>& x)
        {
            double e = 0;
            for (size_t i = 1; i < x.size(); ++i)
                e += (x[i] - x[i - 1]) * (x[i] - x[i - 1]);
            return e;
        };
        CHECK (hf (out.l) < hf (flat.l) * 0.7);
        std::puts ("ok: master EQ high-shelf cut darkens the mix");
    }

    // 6h. MIDI adapt: a Scale-mode 3rd-up voice (C4 -> E4) retunes to the
    // nearest held chord tone (held F major -> F4), and returns to E4 when
    // nothing is held.
    {
        const auto tone = synth ({ { 60, 1.2 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.scaleMode = 1;
        s.midiAdapt = true;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f }; // 3rd up -> E4 normally
        const auto out = runEngine (tone, s, { 65, 69, 72 }); // F major held
        CHECK_NEAR (trackedF0 (out.l, 16384, (int) out.l.size()), midiHz (65), 0.03); // F4
        const auto released = runEngine (tone, s); // nothing held -> diatonic 3rd
        CHECK_NEAR (trackedF0 (released.l, 16384, (int) released.l.size()), midiHz (64), 0.03);
        std::puts ("ok: MIDI adapt snaps layers to held chord, releases to intervals");
    }

    // 6h2. Saturation limits peaks on a voice lane and brightens the master bus.
    {
        const auto tone = synth ({ { 60, 1.2 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.scaleMode = 1;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f };
        const int steady = 16384;
        const int end = (int) tone.size();
        auto hf = [] (const std::vector<float>& x, int a, int b)
        {
            double e = 0, t = 0;
            for (int i = a + 1; i < b; ++i)
                e += (x[(size_t) i] - x[(size_t) i - 1]) * (x[(size_t) i] - x[(size_t) i - 1]);
            for (int i = a; i < b; ++i)
                t += x[(size_t) i] * x[(size_t) i];
            return e / (t + 1e-12);
        };
        auto peak = [] (const std::vector<float>& x, int a, int b)
        {
            float p = 0;
            for (int i = a; i < b; ++i)
                p = std::max (p, std::abs (x[(size_t) i]));
            return p;
        };
        const auto clean = runEngine (tone, s);
        auto satS = s;
        satS.voices[0].satOn = true;
        satS.voices[0].satDrive = 1.0f;
        const auto driven = runEngine (tone, satS);
        // PSOLA harmony is already harmonic-rich; tanh shows up as peak limiting
        // and a non-zero delta, not necessarily higher diff-energy.
        CHECK (peak (driven.l, steady, end) < peak (clean.l, steady, end) * 0.98f);
        double maxDiff = 0.0;
        for (int i = steady; i < end; ++i)
            maxDiff = std::max (maxDiff, (double) std::abs (driven.l[(size_t) i] - clean.l[(size_t) i]));
        CHECK (maxDiff > 1e-4);

        auto mSatS = s;
        mSatS.mSatOn = true;
        mSatS.mSatDrive = 1.0f;
        const auto mDriven = runEngine (tone, mSatS);
        CHECK (hf (mDriven.l, steady, end) > hf (clean.l, steady, end) * 1.02);
        std::puts ("ok: saturation limits voice peaks and brightens master");
    }

    // 6i. Master compressor: engaged on the mix, level clearly changes.
    {
        const auto tone = synth ({ { 60, 1.0 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.scaleMode = 1;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f };
        auto rms = [] (const std::vector<float>& x)
        {
            double e = 0;
            for (float v : x)
                e += v * v;
            return std::sqrt (e / (double) x.size());
        };
        const auto flat = runEngine (tone, s);
        auto compS = s;
        compS.mCompOn = true;
        compS.mCompThresh = -30.0f;
        compS.mCompRatio = 8.0f;
        const auto out = runEngine (tone, compS);
        const double dbDelta = 20.0 * std::log10 (rms (out.l) / (rms (flat.l) + 1e-12));
        CHECK (std::abs (dbDelta) > 1.0);
        std::puts ("ok: master compressor engages on the mix");
    }

    // 6j. Multi-out + voice FX: stems carry the voice's channel chain (an EQ
    // cut on voice 1 shows up in its stem, voice 2's stem untouched).
    {
        const auto tone = synth ({ { 60, 1.2 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.scaleMode = 1;
        s.humanize = 0.0f;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f };
        s.voices[1] = { 1, 11, 57, 1.0f, 0.0f, 0.0f };

        auto runStems = [&] (const HarmonySettings& set, std::vector<float>& v1, std::vector<float>& v2)
        {
            HarmonyEngine eng;
            eng.prepare (kSr, 512);
            eng.setGraph (&chainPlan());
            eng.setSettings (set);
            const size_t len = tone.size();
            std::vector<float> mainL (len), mainR (len), v1r (len), v2r (len);
            v1.assign (len, 0.0f);
            v2.assign (len, 0.0f);
            for (size_t i = 0; i < len; i += 512)
            {
                const int n = (int) std::min<size_t> (512, len - i);
                HarmonyEngine::MultiOut mo;
                mo.mainL = mainL.data() + i;
                mo.mainR = mainR.data() + i;
                mo.voiceL[0] = v1.data() + i;
                mo.voiceR[0] = v1r.data() + i;
                mo.voiceL[1] = v2.data() + i;
                mo.voiceR[1] = v2r.data() + i;
                eng.process (tone.data() + i, mo, n);
            }
        };
        auto rms = [] (const std::vector<float>& x)
        {
            double e = 0;
            for (float v : x)
                e += v * v;
            return std::sqrt (e / (double) x.size());
        };

        std::vector<float> v1a, v2a, v1b, v2b;
        runStems (s, v1a, v2a);
        auto cutS = s;
        cutS.voices[0].eqOn = true;
        cutS.voices[0].eqF[3] = 330.0f;
        cutS.voices[0].eqG[3] = -12.0f;
        runStems (cutS, v1b, v2b);
        CHECK (rms (v1b) < rms (v1a) * 0.75);              // stem 1 reflects its EQ
        CHECK (std::abs (rms (v2b) / rms (v2a) - 1.0) < 0.05); // stem 2 untouched
        std::puts ("ok: stems carry per-voice FX chains");
    }

    // 6k. Signal graph: custom wiring. Two voices summed through one shared
    // EQ node and a GAIN node into OUT; an unwired voice stays silent; a
    // cyclic edge list is rejected.
    {
        const auto tone = synth ({ { 60, 1.0 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.scaleMode = 1;
        s.humanize = 0.0f;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f };  // 3rd up
        s.voices[1] = { 1, 11, 57, 1.0f, 0.0f, 0.0f }; // 5th up
        s.voices[2] = { 1, 14, 57, 1.0f, 0.0f, 0.0f }; // oct up (to be unwired)
        s.gainLevel[0] = 0.5f;

        // V0 + V1 -> EQ0 -> GAIN0(0.5) -> OUT. V2 unwired.
        std::vector<graph::Edge> edges = {
            { graph::kVoice0 + 0, graph::kEq0 },
            { graph::kVoice0 + 1, graph::kEq0 },
            { graph::kEq0, graph::kGain0 },
            { graph::kGain0, graph::kOut },
        };
        const auto plan = graph::compile (edges);
        CHECK (plan.valid);

        auto run = [&] (const graph::Plan* p)
        {
            HarmonyEngine eng;
            eng.prepare (kSr, 512);
            eng.setGraph (p);
            eng.setSettings (s);
            EngineResult out;
            out.l.resize (tone.size());
            out.r.resize (tone.size());
            for (size_t i = 0; i < tone.size(); i += 512)
                eng.process (tone.data() + i, out.l.data() + i, out.r.data() + i,
                             (int) std::min<size_t> (512, tone.size() - i));
            return out;
        };
        auto rms = [] (const std::vector<float>& x)
        {
            double e = 0;
            for (float v : x)
                e += v * v;
            return std::sqrt (e / (double) x.size());
        };

        const auto wired = run (&plan);
        const auto def = run (nullptr); // default patch: all three voices
        CHECK (rms (wired.l) > 0.01);              // audio flows through the custom path
        CHECK (rms (wired.l) < rms (def.l) * 0.85); // gain 0.5 + dropped V2

        // GAIN node actually scales: same wiring at unity is louder.
        auto unity = s;
        unity.gainLevel[0] = 1.0f;
        HarmonyEngine eng2;
        eng2.prepare (kSr, 512);
        eng2.setGraph (&plan);
        eng2.setSettings (unity);
        std::vector<float> ul (tone.size()), ur (tone.size());
        for (size_t i = 0; i < tone.size(); i += 512)
            eng2.process (tone.data() + i, ul.data() + i, ur.data() + i,
                          (int) std::min<size_t> (512, tone.size() - i));
        CHECK (rms (ul) > rms (wired.l) * 1.5);

        // Cycles rejected.
        auto cyc = edges;
        cyc.push_back ({ graph::kGain0, graph::kEq0 });
        CHECK (! graph::compile (cyc).valid);
        std::puts ("ok: signal graph routes, sums, gains, rejects cycles");
    }

    // 6l. ECHO node: repeats ring past the end of the input at the node's
    // time; MIX 0 leaves the lane dry. Cycles through an echo node rejected.
    {
        auto tone = synth ({ { 60, 0.8 } });
        tone.resize (tone.size() + (size_t) (0.6 * kSr), 0.0f); // room for repeats
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.scaleMode = 1;
        s.humanize = 0.0f;
        s.voices[0] = { 1, 9, 57, 1.0f, 0.0f, 0.0f };

        const std::vector<graph::Edge> edges = {
            { graph::kVoice0, graph::kEcho0 },
            { graph::kEcho0, graph::kOut },
        };
        const auto plan = graph::compile (edges);
        CHECK (plan.valid);

        auto rms = [] (const std::vector<float>& x, size_t a, size_t b)
        {
            double e = 0;
            for (size_t i = a; i < b; ++i)
                e += x[i] * x[i];
            return std::sqrt (e / (double) (b - a));
        };
        const size_t n = tone.size();

        auto echoS = s;
        echoS.echo[0] = { 300.0f, 0.5f, 0.9f };
        const auto wet = runEngine (tone, echoS, {}, &plan);
        auto dryS = s;
        dryS.echo[0] = { 300.0f, 0.5f, 0.0f }; // mix 0 = bypass
        const auto dry = runEngine (tone, dryS, {}, &plan);
        const size_t tailA = n - (size_t) (0.3 * kSr);
        CHECK (rms (wet.l, tailA, n) > rms (dry.l, tailA, n) * 4.0 + 1e-4);
        CHECK (rms (dry.l, n / 4, n / 2) > 0.01); // dry still flows through the node

        // A feedback wire through the echo node is still a cycle: rejected.
        std::vector<graph::Edge> cyc = {
            { graph::kVoice0, graph::kEcho0 },
            { graph::kEcho0, graph::kVerb0 },
            { graph::kVerb0, graph::kEcho0 },
        };
        CHECK (! graph::compile (cyc).valid);
        std::puts ("ok: echo node repeats, bypasses at mix 0, rejects cycles");
    }

    // 7. Demo renders (listen: tests_out/*.wav).
    {
        const std::vector<std::pair<double, double>> melody = {
            { 60, 0.4 }, { 62, 0.4 }, { 64, 0.4 }, { 67, 0.6 },
            { 64, 0.4 }, { 62, 0.4 }, { 60, 0.8 },
        };
        const auto lead = synth (melody);
        writeWav ("tests_out/lead_dry.wav", lead);

        HarmonySettings s; // pop/r&b stack: 3rd up L, 5th up R, octave down centre
        s.dryWet = 0.45f;
        s.scaleMode = 1;
        s.keyRoot = 0;
        s.voices[0] = { 1, 9, 57, 0.8f, -0.4f, 0.0f };
        s.voices[1] = { 1, 11, 57, 0.6f, 0.4f, 0.0f };
        s.voices[2] = { 1, 0, 57, 0.5f, 0.0f, 0.0f };
        auto out = runEngine (lead, s);
        writeWav ("tests_out/demo_harmony_diatonic.wav", out.l, out.r);

        // MIDI-mode demo: hold a C major triad above the whole melody.
        HarmonySettings m;
        m.dryWet = 0.45f;
        m.voices[0] = { 3, 7, 57, 0.7f, -0.5f, 0.0f };
        m.voices[1] = { 3, 7, 57, 0.7f, 0.0f, 0.0f };
        m.voices[2] = { 3, 7, 57, 0.7f, 0.5f, 0.0f };
        out = runEngine (lead, m, { 64, 67, 72 });
        writeWav ("tests_out/demo_harmony_midi_chord.wav", out.l, out.r);
        std::puts ("ok: demo WAVs rendered to tests_out/");
    }

    if (failures == 0)
        std::puts ("\nALL TESTS PASSED");
    else
        std::printf ("\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
