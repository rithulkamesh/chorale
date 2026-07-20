// CPU benchmark for the offline HarmonyEngine DSP path.
//   benchmark [blockSize] [seconds] [scenario]
// Scenarios: yin, psola, engine-1v, engine-2v, engine-8v, engine-chain
#include "dsp/HarmonyEngine.h"
#include "dsp/PsolaShifter.h"
#include "dsp/YinTracker.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr double kSr = 44100.0;
constexpr double kPi = 3.14159265358979323846;

using Clock = std::chrono::steady_clock;

double midiHz (double m) { return 440.0 * std::exp2 ((m - 69.0) / 12.0); }

std::vector<float> synthTone (int samples)
{
    std::vector<float> out ((size_t) samples);
    double phase = 0.0;
    const double f = midiHz (60.0);
    for (int i = 0; i < samples; ++i)
    {
        phase += f / kSr;
        double s = 0.0;
        for (int k = 1; k <= 20; ++k)
            if (k * f < kSr / 2.0)
                s += std::sin (2.0 * kPi * k * phase) / k;
        out[(size_t) i] = (float) (0.25 * s);
    }
    return out;
}

struct BenchResult
{
    double usPerBlock = 0.0;
    double rtf = 0.0; // real-time factor: CPU time / audio time
    int blocks = 0;
};

BenchResult bench (const char* name, int blockSize, double seconds, auto&& fn)
{
    const int warmup = (int) (0.25 * kSr / blockSize);
    const int blocks = (int) (seconds * kSr / blockSize);
    for (int i = 0; i < warmup; ++i)
        fn (i);

    const auto t0 = Clock::now();
    for (int i = 0; i < blocks; ++i)
        fn (i);
    const auto t1 = Clock::now();

    const double us = std::chrono::duration<double, std::micro> (t1 - t0).count();
    const double usPerBlock = us / (double) blocks;
    const double blockAudioUs = 1.0e6 * (double) blockSize / kSr;
    BenchResult r;
    r.usPerBlock = usPerBlock;
    r.rtf = usPerBlock / blockAudioUs;
    r.blocks = blocks;
    std::printf ("%-14s  %8.1f us/blk  %6.2f%% CPU  (%d blocks @ %d smp)\n",
                 name, usPerBlock, r.rtf * 100.0, blocks, blockSize);
    return r;
}

void setupVoices (HarmonySettings& s, int count)
{
    s.dryWet = 0.5f;
    s.scaleMode = 1;
    s.humanize = 0.0f;
    for (int v = 0; v < count; ++v)
    {
        s.voices[v].mode = 1; // Scale
        s.voices[v].degree = 7 + (v % 3) * 2; // unison, 3rd, 5th-ish
        s.voices[v].gain = 0.7f;
        s.voices[v].pan = (v % 2 == 0) ? -0.3f : 0.3f;
    }
}

void runScenario (const std::string& scenario, int blockSize, double seconds,
                  const std::vector<float>& in)
{
    if (scenario == "yin")
    {
        YinTracker yin;
        yin.prepare (kSr);
        float frame[YinTracker::kFrame];
        std::memcpy (frame, in.data(), sizeof (frame));
        bench ("yin", blockSize, seconds, [&] (int block)
               {
                   const int off = (block * blockSize) % ((int) in.size() - YinTracker::kFrame);
                   std::memcpy (frame, in.data() + off, sizeof (frame));
                   (void) yin.analyze (frame);
               });
        return;
    }

    if (scenario == "psola")
    {
        PsolaShifter sh[9];
        for (auto& s : sh)
            s.prepare (kSr);
        std::vector<float> tmp ((size_t) blockSize), out ((size_t) blockSize);
        for (auto& s : sh)
        {
            s.setPeriod ((float) (kSr / 261.63), true);
            s.setRatio (1.0f);
        }
        bench ("psola-9x", blockSize, seconds, [&] (int block)
               {
                   const float* src = in.data() + (size_t) ((block * blockSize) % ((int) in.size() - blockSize));
                   for (auto& s : sh)
                       s.process (src, out.data(), blockSize);
               });
        return;
    }

    const graph::Plan* plan = nullptr;
    static const graph::Plan defaultPlan = graph::compile (graph::defaultEdges());
    static const graph::Plan chainPlan = graph::compile (graph::chainEdges());
    if (scenario == "engine-chain")
        plan = &chainPlan;
    else
        plan = &defaultPlan;

    int voiceCount = 1;
    if (scenario == "engine-2v")
        voiceCount = 2;
    else if (scenario == "engine-8v")
        voiceCount = 8;

    HarmonySettings settings;
    setupVoices (settings, voiceCount);
    if (scenario == "engine-chain")
    {
        for (int v = 0; v < voiceCount; ++v)
        {
            settings.voices[v].eqOn = true;
            settings.voices[v].compOn = true;
            settings.voices[v].compThresh = -24.0f;
            settings.voices[v].satOn = true;
            settings.voices[v].satDrive = 0.5f;
        }
    }

    HarmonyEngine eng;
    eng.prepare (kSr, blockSize);
    eng.setGraph (plan);
    eng.setSettings (settings);

    std::vector<float> outL ((size_t) blockSize), outR ((size_t) blockSize);
    const char* label = scenario.c_str();
    bench (label, blockSize, seconds,
           [&] (int block)
           {
               const float* src = in.data() + (size_t) ((block * blockSize) % ((int) in.size() - blockSize));
               eng.process (src, outL.data(), outR.data(), blockSize);
           });
}
} // namespace

int main (int argc, char** argv)
{
    int blockSize = 512;
    double seconds = 3.0;
    std::string scenario = "all";

    if (argc > 1)
        blockSize = std::atoi (argv[1]);
    if (argc > 2)
        seconds = std::atof (argv[2]);
    if (argc > 3)
        scenario = argv[3];

    if (blockSize < 64 || blockSize > 4096)
    {
        std::fprintf (stderr, "block size must be 64..4096\n");
        return 1;
    }

    const int inLen = (int) (seconds * kSr) + YinTracker::kFrame + blockSize;
    const auto in = synthTone (inLen);
    const double blockAudioUs = 1.0e6 * (double) blockSize / kSr;

    std::printf ("Chorale DSP benchmark  sr=%.0f  block=%d  %.1fs  budget=%.1f us/blk\n\n",
                 kSr, blockSize, seconds, blockAudioUs);

    if (scenario == "all")
    {
        runScenario ("yin", blockSize, seconds, in);
        runScenario ("psola", blockSize, seconds, in);
        runScenario ("engine-1v", blockSize, seconds, in);
        runScenario ("engine-2v", blockSize, seconds, in);
        runScenario ("engine-8v", blockSize, seconds, in);
        runScenario ("engine-chain", blockSize, seconds, in);
        std::puts ("\nRTF < 100%% = real-time safe at this block size.");
    }
    else
    {
        runScenario (scenario, blockSize, seconds, in);
    }
    return 0;
}
