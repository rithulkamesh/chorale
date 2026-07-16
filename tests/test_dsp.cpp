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
#include <fstream>
#include <string>
#include <vector>

namespace
{
constexpr double kSr = 44100.0;
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
            const double f0 = f * (1.0 + vibratoDepth * std::sin (2.0 * M_PI * 5.0 * t));
            phase += f0 / kSr;
            double s = 0.0;
            for (int k = 1; k <= 20; ++k)
                if (k * f0 < kSr / 2.0)
                    s += std::sin (2.0 * M_PI * k * phase) / k;
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

EngineResult runEngine (const std::vector<float>& in, const HarmonySettings& s,
                        const std::vector<int>& midiNotes = {})
{
    HarmonyEngine eng;
    eng.prepare (kSr, 512);
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
    system ("mkdir -p tests_out");

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

    // 5. Engine end-to-end, diatonic mode: C4 sung, voice 1 = 3rd Up -> E4.
    {
        const auto tone = synth ({ { 60, 1.2 } });
        HarmonySettings s;
        s.dryWet = 1.0f; // wet only
        s.scaleMode = 1; // manual C major
        s.keyRoot = 0;
        s.voices[0] = { 5, 1.0f, 0.0f }; // 3rd Up
        s.voices[1] = s.voices[2] = s.voices[3] = { 0, 0.0f, 0.0f };
        const auto out = runEngine (tone, s);
        CHECK_NEAR (trackedF0 (out.l, 16384, (int) out.l.size()), midiHz (64), 0.03);
        std::puts ("ok: engine diatonic 3rd-up C4->E4 within 3%");
    }

    // 6. Engine end-to-end, MIDI mode: A3 sung, held E4 -> harmony at E4.
    {
        const auto tone = synth ({ { 57, 1.2 } });
        HarmonySettings s;
        s.dryWet = 1.0f;
        s.midiMode = true;
        s.voices[0] = { 4, 1.0f, 0.0f }; // any non-Off interval; MIDI overrides
        s.voices[1] = s.voices[2] = s.voices[3] = { 0, 0.0f, 0.0f };
        const auto out = runEngine (tone, s, { 64 });
        CHECK_NEAR (trackedF0 (out.l, 16384, (int) out.l.size()), midiHz (64), 0.03);
        std::puts ("ok: engine MIDI mode locks harmony to held E4 within 3%");
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
        s.voices[0] = { 5, 0.8f, -0.4f };
        s.voices[1] = { 6, 0.6f, 0.4f };
        s.voices[2] = { 1, 0.5f, 0.0f };
        s.voices[3] = { 0, 0.0f, 0.0f };
        auto out = runEngine (lead, s);
        writeWav ("tests_out/demo_harmony_diatonic.wav", out.l, out.r);

        // MIDI-mode demo: hold a C major triad above the whole melody.
        HarmonySettings m = s;
        m.midiMode = true;
        m.voices[0] = { 4, 0.7f, -0.5f };
        m.voices[1] = { 4, 0.7f, 0.0f };
        m.voices[2] = { 4, 0.7f, 0.5f };
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
