#include "PsolaShifter.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kPi = 3.14159265358979f;
}

void PsolaShifter::prepare (double)
{
    buf.assign (kSize, 0.0f);
    ola.assign (kSize, 0.0f);
    writeAbs = outAbs = kOrigin;
    nextGrain = markGrid = (double) kOrigin;
    period = 220.0f;
    ratio = targetRatio = 1.0f;
    voiced = false;
}

void PsolaShifter::setPeriod (float p, bool isVoiced)
{
    voiced = isVoiced;
    if (isVoiced)
        period = std::clamp (p, 40.0f, 800.0f);
}

void PsolaShifter::setRatio (float r)
{
    targetRatio = std::clamp (r, 0.25f, 4.0f);
}

void PsolaShifter::process (const float* in, float* out, int n)
{
    for (int i = 0; i < n; ++i)
        buf[(size_t) ((writeAbs++) & kMask)] = in[i];

    // Lay every grain that can still touch this output block. A grain centred
    // at c covers output samples down to c - period, so anything with
    // c <= lastSample + period must exist before we read.
    const double limit = (double) (outAbs + n - 1) + period;
    while (nextGrain <= limit)
    {
        ratio += 0.1f * ((voiced ? targetRatio : 1.0f) - ratio);
        placeGrain (nextGrain);
        nextGrain += period / ratio;
    }

    for (int i = 0; i < n; ++i)
    {
        const size_t idx = (size_t) (outAbs & kMask);
        out[i] = ola[idx];
        ola[idx] = 0.0f;
        ++outAbs;
    }
}

void PsolaShifter::placeGrain (double c)
{
    // Advance the analysis-mark grid (marks spaced one period apart in input
    // time) until it sits nearest the input time corresponding to c.
    while (markGrid < c - kLatency - period * 0.5)
        markGrid += period;

    const int64_t m = (int64_t) std::llround (markGrid);
    const int64_t oc = (int64_t) std::llround (c);
    const int T = std::max (16, (int) std::lround (period));
    const float norm = 1.0f / std::max (1.0f, ratio); // Hann OLA at T/ratio spacing sums to ~ratio

    for (int k = -T; k <= T; ++k)
    {
        const float w = 0.5f * (1.0f + std::cos (kPi * (float) k / (float) T));
        ola[(size_t) ((oc + k) & kMask)] += norm * w * buf[(size_t) ((m + k) & kMask)];
    }
}
