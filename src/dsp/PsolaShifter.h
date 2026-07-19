#pragma once

#include <cstdint>
#include <vector>

// Streaming TD-PSOLA pitch shifter. Grains of 2 periods are taken from the
// input at period spacing and overlap-added at period/ratio spacing; grains
// are never resampled, so the spectral envelope (formants) is preserved.
// ponytail: analysis marks follow a period grid rather than true glottal
// epochs — snap-to-epoch is the quality upgrade if roughness shows on real
// vocals. Independent formant *shifting* needs the World vocoder path.
class PsolaShifter
{
public:
    static constexpr int kLatency = 2048;     // studio default (~46 ms @ 44.1k)
    static constexpr int kLiveLatency = 1024; // live mode (~23 ms @ 44.1k)

    void prepare (double sampleRate);
    // Grain lookbehind; clean PSOLA needs latency >= ~2.5 periods, so lower
    // latency also lowers the longest usable period (see setPeriod).
    void setLatency (int samples) { latency = samples; }
    void setPeriod (float periodSamples, bool isVoiced);
    void setRatio (float ratio); // pitch ratio target, >1 = up
    void process (const float* in, float* out, int n);

private:
    void placeGrain (double centre);

    static constexpr int kSize = 1 << 15;
    static constexpr int kMask = kSize - 1;
    static constexpr int64_t kOrigin = int64_t (1) << 40; // keeps indices positive for & kMask

    std::vector<float> buf, ola;
    int latency = kLatency;
    int64_t writeAbs = 0, outAbs = 0;
    double nextGrain = 0, markGrid = 0;
    float period = 220.0f, targetRatio = 1.0f, ratio = 1.0f;
    bool voiced = false;
};
