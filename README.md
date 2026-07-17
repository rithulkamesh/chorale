# Chorale

**Open-source vocal harmonizer & stacker (VST3 / AU / Standalone — macOS, Windows, Linux).**
Sing one line, get a full vocal production: harmony stacks, pedal notes,
MIDI-driven chords, pitch correction, doubling — eight formant-preserving
voices from a single mono vocal.

![Chorale UI](docs/screenshot.png)

## How it works

```
mono vocal ──> pitch detection ──> key/scale engine ──> per-voice target pitch
   (YIN)      (auto or manual)     (Krumhansl-Schmuckler)        │
     │                                                           v
     ├──> lead correction (PSOLA)          8x TD-PSOLA pitch shifters
     v                                                           v
   lead (latency-aligned) ────> mix <── tone / width / echo <── solo·mute·pan·level
```

Pitch shifting is time-domain PSOLA: grains are never resampled, so the
singer's formants (vocal character) are preserved instead of chipmunked.

## Features

- **8 harmony voices**, each in one of three modes:
  - **Scale** — diatonic interval from the sung note (2nd–octave, up/down),
    snapped to the key so 3rds come out major or minor as the key demands
  - **Note** — the voice holds one fixed pitch while you move: alto pedals,
    drones, static chord pads
  - **MIDI** — the voice tracks notes you hold on a keyboard
- Per-voice **level, pan, detune** (±50 cents), **solo & mute** — audition any
  voice in isolation with one click
- **Lead pitch correction**: Off / Natural (partial, musical) / Hard (full snap)
- **Humanize**: slow decorrelated pitch drift + level flutter per voice, so
  stacks sound like singers, not a rack unit
- **Wet-bus FX**: tone (low-pass), stereo width, ping-pong echo with feedback
- **Auto key detection** (Krumhansl-Schmuckler) or manual root + mode
  (major, minor, church modes, chromatic)
- **33 stock presets** across Duets, Stacks, Choirs, Octaves, Doublers,
  Pedals, MIDI, Experimental — fully editable after applying; presets never
  touch your mix setting
- Radar stage UI: drag a voice bubble to set pan (horizontal) and level
  (vertical), with a fine audio-reactive particle field; live keyboard strip
  shows the lead note and every harmony target
- Latency (2048 samples ≈ 46 ms @ 44.1k) reported to the host for automatic
  compensation; AU passes `auval`

## Demos

Synthesized renders in [`demos/`](demos/) (`lead_dry.wav`,
`demo_harmony_diatonic.wav`, `demo_harmony_midi_chord.wav`), or render your
own vocal offline without a DAW:

```sh
build/harmonize in.wav out.wav [dryWet] [key|auto] [scale|auto] [wetonly]
```

Reads PCM 16/24/32 or float WAV, prints the detected key, runs ~10× realtime.

## Building

Requires CMake ≥ 3.24 and a C++20 compiler. JUCE is fetched automatically.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Artifacts land in `build/Chorale_artefacts/Release/`. On macOS copy the
`.component` to `~/Library/Audio/Plug-Ins/Components/` and/or the `.vst3` to
`~/Library/Audio/Plug-Ins/VST3/`; on Windows copy the `.vst3` to
`C:\Program Files\Common Files\VST3`; on Linux to `~/.vst3`.

On Linux install the JUCE build dependencies first:

```sh
sudo apt-get install libasound2-dev libx11-dev libxext-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev
```

## Releases, signing, notarization

CI builds and tests every push on macOS, Windows, and Linux
(`.github/workflows/build.yml`). Pushing a `v*` tag builds release zips for
all three platforms and attaches them to a GitHub Release
(`.github/workflows/release.yml`); the macOS build is a universal binary
(arm64 + x86_64) and is codesigned + notarized + stapled automatically when
the signing secrets are configured (see the comment at the top of
`release.yml`). To notarize a local build:

```sh
./scripts/notarize.sh build/Chorale_artefacts/Release \
  you@appleid.com TEAMID1234 app-specific-password
```

## Testing

```sh
cmake --build build --target dsp_tests && ./build/dsp_tests
```

The DSP chain is pure C++ (no JUCE) and is tested closed-loop against
synthesized vocal-like signals: the harmonized output is pitch-tracked and
asserted to land on the target interval. Covers the YIN tracker, the interval
calculator, key detection, the PSOLA shifter, all three voice modes, solo
isolation, and hard pitch correction.

## Architecture

```
src/
  dsp/                  JUCE-free, unit-testable DSP core
    PitchTracker.h      tracker interface (CREPE/ONNX drops in behind this)
    YinTracker          YIN + CMND + parabolic interpolation
    KeyEngine           K-S key finding + diatonic interval calculator
    PsolaShifter        streaming TD-PSOLA, formant-preserving
    HarmonyEngine       full chain: tracker -> key -> voices -> bus FX -> mix
  PluginProcessor       JUCE wrapper, parameters, telemetry
  PluginEditor          radar stage UI, particles, keyboard, detail panel
  Presets.h             stock harmony shapes
tools/harmonize.cpp     offline CLI (WAV in -> WAV out)
tests/test_dsp.cpp      offline closed-loop tests
scripts/notarize.sh     macOS codesign + notarytool + staple
```

## Roadmap

- CREPE pitch tracking via ONNX Runtime (higher accuracy on noisy vocals)
- World vocoder shift mode (independent formant *shifting*) + formant knob
- Epoch-snapped PSOLA analysis marks (quality at larger shift ratios)
- Tempo-synced echo; latency reduction / configurable lookahead

## License

[AGPL-3.0](LICENSE). Chorale is built on [JUCE](https://juce.com), which is
available under AGPLv3 for open-source projects — hence this project's
license. Planned integrations are compatible: World vocoder (BSD-3-Clause),
ONNX Runtime (MIT). CREPE model weights have separate distribution terms and
would ship as a user download, not bundled.
