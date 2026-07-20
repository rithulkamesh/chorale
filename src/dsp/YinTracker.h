#pragma once

#include "PitchTracker.h"
#include "Fft.h"
#include <vector>

// YIN with cumulative-mean-normalised difference, absolute-threshold first
// minimum, and parabolic interpolation (de Cheveigne & Kawahara 2002).
// Difference function via FFT autocorrelation (O(N log N) vs O(W*tau)).
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
    dsp::Fft<4096> fft;
    std::vector<float> cmnd, xr;
};
