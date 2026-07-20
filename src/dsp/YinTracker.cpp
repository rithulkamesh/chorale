#include "YinTracker.h"
#include "Fft.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr int kFftSize = 4096; // >= W + (W + tauMax) for linear cross-corr
static_assert (YinTracker::kFrame / 2 + YinTracker::kFrame / 2 + 735 <= kFftSize,
               "FFT size too small for YIN frame");
} // namespace

void YinTracker::prepare (double sampleRate)
{
    sr = sampleRate;
    tauMin = std::max (2, (int) (sr / 1000.0));           // fmax 1 kHz
    tauMax = std::min (kFrame / 2 - 2, (int) (sr / 60.0)); // fmin 60 Hz
    cmnd.assign ((size_t) tauMax + 1, 1.0f);
    xr.assign ((size_t) tauMax + 1, 0.0f);
}

PitchEstimate YinTracker::analyze (const float* x)
{
    const int W = kFrame / 2;

    double energy = 0.0;
    for (int i = 0; i < kFrame; ++i)
        energy += x[i] * x[i];
    if (std::sqrt (energy / kFrame) < 0.005) // ~ -46 dBFS gate
        return {};

    // Difference function: d(tau) = E0 + E1(tau) - 2 * r(tau),
    // r(tau) = sum_{i=0}^{W-1} x[i]*x[i+tau]  (FFT cross-correlation).
    fft.crosscorr (x, W, x, W + tauMax, tauMax, xr.data());

    float e0 = 0.0f;
    for (int i = 0; i < W; ++i)
        e0 += x[i] * x[i];

    float e1 = 0.0f;
    for (int j = 1; j < W + 1; ++j)
        e1 += x[j] * x[j];

    cmnd[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        const float d = e0 + e1 - 2.0f * xr[(size_t) tau];
        running += d;
        cmnd[(size_t) tau] = running > 0.0 ? (float) (d * (double) tau / running) : 1.0f;
        if (tau + W < kFrame)
            e1 += x[tau + W] * x[tau + W] - x[tau] * x[tau];
    }

    int best = -1;
    constexpr float threshold = 0.15f;
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (cmnd[(size_t) tau] < threshold)
        {
            while (tau + 1 <= tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0)
    {
        best = tauMin;
        for (int tau = tauMin; tau <= tauMax; ++tau)
            if (cmnd[(size_t) tau] < cmnd[(size_t) best])
                best = tau;
    }

    const float cm = cmnd[(size_t) best];

    double tau = best;
    if (best > tauMin && best < tauMax)
    {
        const double a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1e-12)
            tau = best + 0.5 * (a - c) / den;
    }

    PitchEstimate est;
    est.f0 = (float) (sr / tau);
    est.confidence = std::max (0.0f, 1.0f - cm);
    est.voiced = cm < 0.3f && est.f0 >= 60.0f && est.f0 <= 1000.0f;
    return est;
}
