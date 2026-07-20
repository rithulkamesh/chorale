#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// Per-voice / master channel FX primitives. JUCE-free (offline-testable).
// RBJ cookbook biquads; magDb() exists so the editor can plot the same curve
// the audio path applies.

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setIdentity()
    {
        b0 = 1; b1 = b2 = a1 = a2 = 0;
    }

    void setLowShelf (float sr, float f, float dB)
    {
        shelf (sr, f, dB, true);
    }

    void setHighShelf (float sr, float f, float dB)
    {
        shelf (sr, f, dB, false);
    }

    void setPeak (float sr, float f, float dB, float q = 0.8f)
    {
        if (std::abs (dB) < 0.01f) { setIdentity(); return; }
        const float A = std::pow (10.0f, dB / 40.0f);
        const float w = 2.0f * 3.14159265f * std::clamp (f, 20.0f, 0.45f * sr) / sr;
        const float alpha = std::sin (w) / (2.0f * q);
        const float c = std::cos (w);
        const float a0 = 1 + alpha / A;
        b0 = (1 + alpha * A) / a0;
        b1 = (-2 * c) / a0;
        b2 = (1 - alpha * A) / a0;
        a1 = (-2 * c) / a0;
        a2 = (1 - alpha / A) / a0;
    }

    // Magnitude response in dB at frequency f — used by the EQ graph UI.
    // Double math: at low frequencies (cos w -> 1) the num/den terms cancel
    // almost completely and float precision collapses to garbage.
    float magDb (float sr, float f) const
    {
        const double B0 = b0, B1 = b1, B2 = b2, A1 = a1, A2 = a2;
        const double w = 2.0 * 3.141592653589793 * (double) f / (double) sr;
        const double cw = std::cos (w), c2w = std::cos (2.0 * w);
        const double num = B0 * B0 + B1 * B1 + B2 * B2
                           + 2.0 * (B0 * B1 + B1 * B2) * cw + 2.0 * B0 * B2 * c2w;
        const double den = 1.0 + A1 * A1 + A2 * A2
                           + 2.0 * (A1 + A1 * A2) * cw + 2.0 * A2 * c2w;
        return (float) (10.0 * std::log10 (std::max (num, 1e-14) / std::max (den, 1e-14)));
    }

private:
    void shelf (float sr, float f, float dB, bool low)
    {
        if (std::abs (dB) < 0.01f) { setIdentity(); return; }
        const float A = std::pow (10.0f, dB / 40.0f);
        const float w = 2.0f * 3.14159265f * std::clamp (f, 20.0f, 0.45f * sr) / sr;
        const float c = std::cos (w);
        const float alpha = std::sin (w) / 2.0f * std::sqrt (2.0f); // S = 1
        const float sq = 2.0f * std::sqrt (A) * alpha;
        const float sgn = low ? 1.0f : -1.0f;
        const float a0 = (A + 1) + sgn * (A - 1) * c + sq;
        b0 = A * ((A + 1) - sgn * (A - 1) * c + sq) / a0;
        b1 = sgn * 2 * A * ((A - 1) - sgn * (A + 1) * c) / a0;
        b2 = A * ((A + 1) - sgn * (A - 1) * c - sq) / a0;
        a1 = sgn * -2 * ((A - 1) + sgn * (A + 1) * c) / a0;
        a2 = ((A + 1) + sgn * (A - 1) * c - sq) / a0;
    }
};

// Multiband channel EQ: band 0 = low shelf, band kBands-1 = high shelf,
// everything between = peaks. All bands are full-range (FabFilter-style).
struct ChannelEq
{
    static constexpr int kBands = 8;
    Biquad b[kBands];
    bool active = false;

    // freqs/gains are kBands-long arrays (Hz / dB).
    void set (float sr, const float* freqs, const float* gains)
    {
        active = false;
        for (int i = 0; i < kBands; ++i)
            active = active || std::abs (gains[i]) > 0.01f;
        b[0].setLowShelf (sr, freqs[0], gains[0]);
        for (int i = 1; i < kBands - 1; ++i)
            b[i].setPeak (sr, freqs[i], gains[i], 1.0f);
        b[kBands - 1].setHighShelf (sr, freqs[kBands - 1], gains[kBands - 1]);
    }

    void reset()
    {
        for (auto& q : b)
            q.reset();
    }

    inline float process (float x)
    {
        for (auto& q : b)
            x = q.process (x);
        return x;
    }
};

// Feed-forward compressor, fixed 5 ms attack / 120 ms release, gentle auto
// makeup. threshDb >= -0.5 means bypassed.
struct Compressor
{
    float env = 0, aAtt = 0, aRel = 0;
    float grDb = 0; // last gain reduction (negative dB, excl. makeup) for UI

    void prepare (double sr)
    {
        aAtt = (float) std::exp (-1.0 / (0.005 * sr));
        aRel = (float) std::exp (-1.0 / (0.120 * sr));
        env = 0;
        grDb = 0;
    }

    inline float process (float x, float threshDb, float ratio)
    {
        const float lvl = std::abs (x);
        env = lvl > env ? aAtt * env + (1 - aAtt) * lvl
                        : aRel * env + (1 - aRel) * lvl;
        const float envDb = 20.0f * std::log10 (std::max (env, 1e-6f));
        const float over = envDb - threshDb;
        if (over <= 0.0f)
        {
            grDb = 0.0f;
            return x;
        }
        const float inv = 1.0f - 1.0f / std::max (ratio, 1.01f);
        grDb = -over * inv;
        const float gainDb = grDb - threshDb * inv * 0.4f; // reduction + auto makeup
        return x * std::pow (10.0f, gainDb / 20.0f);
    }
};

// Stateless tanh waveshaper with rough level compensation and dry/wet.
// ponytail: sqrt(g) compensation is a compromise — small signals gain up to
// +12 dB at full drive, peaks land ~-12 dB; oversampling is the upgrade if
// aliasing shows on bright material.
struct Saturator
{
    static inline float shape (float x, float drive) // drive 0..1
    {
        const float g = std::exp2 (drive * 4.0f); // 1..16
        return std::tanh (g * x) / std::sqrt (g);
    }

    static inline float process (float x, float drive, float mix)
    {
        return x + (shape (x, drive) - x) * mix;
    }
};

// Freeverb-style reverb: 8 combs + 4 allpasses per channel, mono in,
// stereo out. ponytail: classic tuning, no modulation — an FDN with
// modulated delays is the upgrade if metallic ringing shows up.
struct SimpleReverb
{
    void prepare (double sr)
    {
        static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apT[4] = { 556, 441, 341, 225 };
        const float scale = (float) (sr / 44100.0);
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 8; ++i)
            {
                comb[c][i].assign ((size_t) std::max (8, (int) ((float) combT[i] * scale)) + (c == 1 ? 23 : 0), 0.0f);
                combPos[c][i] = 0;
                combStore[c][i] = 0;
            }
            for (int i = 0; i < 4; ++i)
            {
                ap[c][i].assign ((size_t) std::max (8, (int) ((float) apT[i] * scale)) + (c == 1 ? 23 : 0), 0.0f);
                apPos[c][i] = 0;
            }
        }
    }

    void setSize (float s) { feedback = 0.74f + 0.24f * std::clamp (s, 0.0f, 1.0f); }

    inline void process (float in, float& outL, float& outR)
    {
        const float input = in * 0.015f;
        float out[2] = { 0, 0 };
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 8; ++i)
            {
                auto& buf = comb[c][i];
                float& store = combStore[c][i];
                int& pos = combPos[c][i];
                const float y = buf[(size_t) pos];
                store = y * (1.0f - damp) + store * damp;
                buf[(size_t) pos] = input + store * feedback;
                if (++pos >= (int) buf.size())
                    pos = 0;
                out[c] += y;
            }
            for (int i = 0; i < 4; ++i)
            {
                auto& buf = ap[c][i];
                int& pos = apPos[c][i];
                const float y = buf[(size_t) pos];
                buf[(size_t) pos] = out[c] + y * 0.5f;
                out[c] = y - out[c];
                if (++pos >= (int) buf.size())
                    pos = 0;
            }
        }
        outL = out[0];
        outR = out[1];
    }

private:
    std::vector<float> comb[2][8], ap[2][4];
    int combPos[2][8] {}, apPos[2][4] {};
    float combStore[2][8] {};
    float feedback = 0.86f, damp = 0.4f;
};
