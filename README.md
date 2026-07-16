# OpenHarmony

Open-source real-time vocal harmonizer plugin (VST3/AU), built with JUCE.

Signal chain: mono vocal → pitch detection → key/scale-aware interval
calculation → formant-preserving pitch shift per harmony voice → per-voice
level/pan → mix with dry signal.

## Status

- [x] 1. JUCE plugin shell
- [x] 2. YIN pitch tracking (CMND + parabolic interp; pYIN Viterbi = upgrade path)
- [x] 3. Key/scale engine (Krumhansl-Schmuckler auto-detect, manual root+mode,
       diatonic interval calculator, all modes + chromatic)
- [x] 4. Streaming TD-PSOLA formant-preserving pitch shifting
- [x] 5. 4 parallel harmony voices, per-voice interval/gain/pan, latency-aligned dry/wet
- [ ] 6. CREPE via ONNX Runtime (drops in behind the `PitchTracker` interface)
- [ ] 7. World vocoder mode (independent formant shift; PSOLA preserves formants but can't move them)
- [x] 8. MIDI-driven harmony (held notes = absolute target pitches, overrides diatonic calc)
- [ ] 9. Custom UI (currently generic parameter editor) + latency reduction (fixed 2048 samples ≈ 46 ms, host-compensated)

## Testing

`build/dsp_tests` runs the JUCE-free DSP chain offline against synthesized
vocal-like signals (band-limited harmonic tones with vibrato) and verifies
closed-loop: the harmonized output is pitch-tracked and asserted to land on
the target interval. Demo renders land in `build/tests_out/*.wav` — listen to
`demo_harmony_diatonic.wav` (3rd up / 5th up / octave down stack) and
`demo_harmony_midi_chord.wav` (held C-major triad driving the voices).

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

JUCE is fetched automatically via CMake FetchContent. Outputs land in
`build/OpenHarmony_artefacts/` (VST3, AU component, standalone app).

## Licensing notes (for planned integrations)

- **JUCE**: AGPLv3 for open-source use — this project must be
  AGPLv3-compatible (or use a JUCE commercial license).
- **World vocoder**: BSD 3-clause — fine.
- **ONNX Runtime**: MIT — fine.
- **CREPE weights**: the CREPE code is MIT, but the pretrained model weights
  were trained on datasets with mixed licensing; ship weights as a separate
  download and verify redistribution terms before bundling.
