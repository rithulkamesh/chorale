// Offline harmonizer CLI: run the real HarmonyEngine over a WAV file.
//   harmonize <in.wav> <out.wav> [dryWet 0..1] [key: C..B|auto] [scale name] [wetonly]
// Reads PCM 16/24/32 or float32, any channel count (downmixed to mono).
#include "dsp/HarmonyEngine.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace
{
struct Wav
{
    double sr = 0;
    std::vector<float> mono;
};

bool readWav (const std::string& path, Wav& w)
{
    std::ifstream f (path, std::ios::binary);
    if (! f)
        return false;
    char id[4];
    uint32_t sz;
    f.read (id, 4); f.read ((char*) &sz, 4); f.read (id, 4); // RIFF..WAVE
    if (std::memcmp (id, "WAVE", 4) != 0)
        return false;

    uint16_t fmt = 0, ch = 0, bits = 0;
    uint32_t rate = 0;
    while (f.read (id, 4) && f.read ((char*) &sz, 4))
    {
        if (! std::memcmp (id, "fmt ", 4))
        {
            std::vector<char> buf (sz);
            f.read (buf.data(), sz);
            fmt = *(uint16_t*) &buf[0];
            ch = *(uint16_t*) &buf[2];
            rate = *(uint32_t*) &buf[4];
            bits = *(uint16_t*) &buf[14];
            if (fmt == 0xFFFE && sz >= 40) // WAVE_FORMAT_EXTENSIBLE: real format in SubFormat GUID
                fmt = *(uint16_t*) &buf[24];
        }
        else if (! std::memcmp (id, "data", 4))
        {
            std::vector<uint8_t> raw (sz);
            f.read ((char*) raw.data(), sz);
            const int bytes = bits / 8;
            const size_t frames = sz / (size_t) (bytes * ch);
            w.mono.resize (frames);
            for (size_t i = 0; i < frames; ++i)
            {
                double acc = 0;
                for (int c = 0; c < ch; ++c)
                {
                    const uint8_t* p = raw.data() + (i * ch + c) * bytes;
                    double v = 0;
                    if (fmt == 3 && bits == 32)
                        v = *(const float*) p;
                    else if (bits == 16)
                        v = *(const int16_t*) p / 32768.0;
                    else if (bits == 24)
                    {
                        int32_t s = (p[0] << 8) | (p[1] << 16) | ((int32_t) (int8_t) p[2] << 24);
                        v = (s >> 8) / 8388608.0;
                    }
                    else if (bits == 32)
                        v = *(const int32_t*) p / 2147483648.0;
                    acc += v;
                }
                w.mono[i] = (float) (acc / ch);
            }
            w.sr = rate;
            return frames > 0;
        }
        else
            f.seekg (sz + (sz & 1), std::ios::cur);
    }
    return false;
}

void writeWav (const std::string& path, double sr, const std::vector<float>& l, const std::vector<float>& r)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t frames = (uint32_t) l.size(), dataBytes = frames * 4;
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    f.write ("RIFF", 4); u32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (1); u16 (2);
    u32 ((uint32_t) sr); u32 ((uint32_t) sr * 4); u16 (4); u16 (16);
    f.write ("data", 4); u32 (dataBytes);
    for (uint32_t i = 0; i < frames; ++i)
    {
        const float sl = std::fmin (1.f, std::fmax (-1.f, l[i]));
        const float sr2 = std::fmin (1.f, std::fmax (-1.f, r[i]));
        u16 ((uint16_t) (int16_t) std::lround (sl * 32767.f));
        u16 ((uint16_t) (int16_t) std::lround (sr2 * 32767.f));
    }
}

const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* kScaleNames[9] = { "auto", "major", "minor", "dorian", "phrygian",
                               "lydian", "mixolydian", "locrian", "chromatic" };
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf (stderr,
                      "usage: %s in.wav out.wav [dryWet=0.5] [key=auto] [scale=auto] [wetonly]\n",
                      argv[0]);
        return 2;
    }

    Wav in;
    if (! readWav (argv[1], in))
    {
        std::fprintf (stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    std::printf ("in: %.1fs @ %.0f Hz\n", in.mono.size() / in.sr, in.sr);

    HarmonySettings s;
    s.dryWet = argc > 3 ? std::stof (argv[3]) : 0.5f;
    if (argc > 5)
        for (int i = 1; i < 9; ++i)
            if (! std::strcmp (argv[5], kScaleNames[i]))
                s.scaleMode = i;
    if (argc > 4 && std::strcmp (argv[4], "auto") != 0)
    {
        for (int i = 0; i < 12; ++i)
            if (! std::strcmp (argv[4], kNoteNames[i]))
                s.keyRoot = i;
        if (s.scaleMode == 0)
            s.scaleMode = 1; // manual key implies at least major unless scale given
    }
    // Default stack: 3rd up L, 5th up R, octave down centre (muted-ish).
    s.voices[0] = { 5, 0.8f, -0.4f };
    s.voices[1] = { 6, 0.6f, 0.4f };
    s.voices[2] = { 1, 0.35f, 0.0f };
    s.voices[3] = { 0, 0.0f, 0.0f };
    if (argc > 6 && ! std::strcmp (argv[6], "wetonly"))
        s.dryWet = 1.0f;

    HarmonyEngine eng;
    constexpr int kBlock = 512;
    eng.prepare (in.sr, kBlock);
    eng.setSettings (s);

    std::vector<float> outL (in.mono.size()), outR (in.mono.size());
    int voicedHops = 0;
    for (size_t i = 0; i < in.mono.size(); i += kBlock)
    {
        const int n = (int) std::min<size_t> (kBlock, in.mono.size() - i);
        eng.process (in.mono.data() + i, outL.data() + i, outR.data() + i, n);
        if (eng.lastPitch().voiced)
            ++voicedHops;
    }

    writeWav (argv[2], in.sr, outL, outR);
    std::printf ("out: %s\nvoiced blocks: %.0f%%\ndetected key: %s %s\n",
                 argv[2], 100.0 * voicedHops / ((double) in.mono.size() / kBlock),
                 kNoteNames[eng.detectedRootPc()], eng.detectedMinor() ? "minor" : "major");
    return 0;
}
