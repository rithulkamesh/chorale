#include "KeyEngine.h"
#include <cmath>

namespace
{
// Krumhansl-Schmuckler probe-tone profiles.
const float kMajorProfile[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f,
                                  2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
const float kMinorProfile[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f,
                                  2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

const int kScaleLen[8] = { 7, 7, 7, 7, 7, 7, 7, 12 };
const int kScales[8][12] = {
    { 0, 2, 4, 5, 7, 9, 11 },                 // major
    { 0, 2, 3, 5, 7, 8, 10 },                 // natural minor
    { 0, 2, 3, 5, 7, 9, 10 },                 // dorian
    { 0, 1, 3, 5, 7, 8, 10 },                 // phrygian
    { 0, 2, 4, 6, 7, 9, 11 },                 // lydian
    { 0, 2, 4, 5, 7, 9, 10 },                 // mixolydian
    { 0, 1, 3, 5, 6, 8, 10 },                 // locrian
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, // chromatic
};

int floorDiv (int a, int b)
{
    const int q = a / b, r = a % b;
    return (r != 0 && (r < 0) != (b < 0)) ? q - 1 : q;
}

float correlate (const float* chroma, const float* profile, int rotation)
{
    // Pearson correlation of chroma against profile rotated to `rotation` root.
    float mc = 0, mp = 0;
    for (int i = 0; i < 12; ++i)
    {
        mc += chroma[i];
        mp += profile[i];
    }
    mc /= 12;
    mp /= 12;
    float num = 0, dc = 0, dp = 0;
    for (int i = 0; i < 12; ++i)
    {
        const float c = chroma[i] - mc;
        const float p = profile[(i - rotation + 12) % 12] - mp;
        num += c * p;
        dc += c * c;
        dp += p * p;
    }
    const float den = std::sqrt (dc * dp);
    return den > 1e-9f ? num / den : 0.0f;
}
} // namespace

void KeyEngine::reset()
{
    for (auto& c : chroma)
        c = 0;
    root = 0;
    minorKey = false;
    sinceDetect = 0;
}

void KeyEngine::addObservation (float f0, float weight)
{
    const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (f0 / 440.0));
    const int pc = ((midi % 12) + 12) % 12;
    for (auto& c : chroma)
        c *= 0.999f; // ~ rolling window of a few thousand hops
    chroma[pc] += weight;
    if (++sinceDetect >= 16)
    {
        detect();
        sinceDetect = 0;
    }
}

void KeyEngine::detect()
{
    float best = -2.0f;
    for (int r = 0; r < 12; ++r)
    {
        const float cMaj = correlate (chroma, kMajorProfile, r);
        const float cMin = correlate (chroma, kMinorProfile, r);
        if (cMaj > best)
        {
            best = cMaj;
            root = r;
            minorKey = false;
        }
        if (cMin > best)
        {
            best = cMin;
            root = r;
            minorKey = true;
        }
    }
}

int KeyEngine::harmonyTarget (int noteMidi, int rootPc, int scaleIdx, int degreeOffset)
{
    const int len = kScaleLen[scaleIdx];
    const int* sc = kScales[scaleIdx];
    const int rel = noteMidi - rootPc;
    const int baseOct = floorDiv (rel, 12);

    int bestDeg = 0, bestErr = 1 << 30;
    for (int k = -1; k <= 1; ++k)
        for (int i = 0; i < len; ++i)
        {
            const int cand = sc[i] + 12 * (baseOct + k);
            const int err = std::abs (cand - rel);
            if (err < bestErr)
            {
                bestErr = err;
                bestDeg = i + len * (baseOct + k);
            }
        }

    const int t = bestDeg + degreeOffset;
    const int oct = floorDiv (t, len);
    return rootPc + sc[t - oct * len] + 12 * oct;
}
