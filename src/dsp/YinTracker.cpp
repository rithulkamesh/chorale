#include "YinTracker.h"
#include <algorithm>
#include <cmath>

void YinTracker::prepare (double sampleRate)
{
    sr = sampleRate;
    tauMin = std::max (2, (int) (sr / 1000.0));           // fmax 1 kHz
    tauMax = std::min (kFrame / 2 - 2, (int) (sr / 60.0)); // fmin 60 Hz
    d.assign ((size_t) tauMax + 1, 0.0f);
    cmnd.assign ((size_t) tauMax + 1, 1.0f);
}

PitchEstimate YinTracker::analyze (const float* x)
{
    const int W = kFrame / 2;

    double energy = 0.0;
    for (int i = 0; i < kFrame; ++i)
        energy += x[i] * x[i];
    if (std::sqrt (energy / kFrame) < 0.005) // ~ -46 dBFS gate
        return {};

    for (int tau = 1; tau <= tauMax; ++tau)
    {
        double sum = 0.0;
        for (int i = 0; i < W; ++i)
        {
            const float diff = x[i] - x[i + tau];
            sum += diff * diff;
        }
        d[(size_t) tau] = (float) sum;
    }

    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += d[(size_t) tau];
        cmnd[(size_t) tau] = running > 0.0 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
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
