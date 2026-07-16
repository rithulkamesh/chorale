#pragma once

#include "PitchTracker.h"
#include <vector>

// YIN with cumulative-mean-normalised difference, absolute-threshold first
// minimum, and parabolic interpolation (de Cheveigne & Kawahara 2002).
// ponytail: full pYIN adds beta-weighted multi-threshold candidates + HMM
// Viterbi; add if octave errors show up on real vocals. Direct O(W*tau)
// difference function; switch to FFT autocorrelation if CPU matters.
class YinTracker : public PitchTracker
{
public:
    static constexpr int kFrame = 2048;

    void prepare (double sampleRate) override;
    int frameSize() const override { return kFrame; }
    PitchEstimate analyze (const float* frame) override;

private:
    double sr = 44100.0;
    int tauMin = 44, tauMax = 735;
    std::vector<float> d, cmnd;
};
