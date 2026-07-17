# Contributing to Chorale

- **Bugs / features**: open an issue with your OS, DAW, plugin format, and
  steps to reproduce. For audio quality issues, attach a short dry WAV that
  demonstrates it — `build/harmonize` lets you reproduce offline.
- **DSP changes** must come with a closed-loop test in `tests/test_dsp.cpp`
  (synthesize a signal, run the chain, pitch-track the output, assert). Run
  `./build/dsp_tests` before opening a PR — CI runs it on macOS, Windows,
  and Linux.
- **Keep the DSP core JUCE-free.** `src/dsp/` must compile with a bare C++20
  toolchain; anything JUCE-flavoured belongs in the plugin wrapper or editor.
- Code style follows the existing files (JUCE-ish: 4-space indent, braces on
  their own line, `camelCase`). Build with the default warning flags —
  warnings are treated as review blockers.
- By contributing you agree your work is licensed under AGPL-3.0.
