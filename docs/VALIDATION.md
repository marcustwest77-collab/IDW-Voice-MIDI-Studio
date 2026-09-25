# IDW Voice MIDI Studio V4 - validation

Built on Windows x64 on September 22, 2026 using the supplied JUCE 9.0.2 checkout and MSVC 19.44.35229. Windows app and VST3 use a static C++ runtime.

## Passed

- Native Release build: standalone EXE, VST3 bundle and regression executable.
- Pitch/onset/release consistency at 44.1, 48 and 96 kHz across 64, 128, 256 and 512-sample host buffers.
- Synthetic pitch accuracy checks from 70 to 990 Hz.
- Strict scale pitch-bend centering and bounded natural expression.
- MPE mode changes release the original channel; Panic and test-note pairing.
- Multiple CC messages per block; channel isolation; saved mapping restoration.
- Capture timestamp order, closing events, and continuing live sound after recording stops.
- MIDI export round-trip: readable file, 960 PPQN, tempo, bends and explicit held-note closure.
- Five-hit drum training, profile persistence and recognition of a matching synthetic hit.
- Nonzero preview audio, right-channel microphone analysis and default raw-input muting.
- Editor creation preserves all parameter values.
- Default (1120x900) and minimum (1040x890) editor renderings; all visible controls inside bounds; images visually inspected.
- pluginval 1.0.4, strictness level 5: SUCCESS. Includes parameter/state, automation, editor and bus-layout checks.

## Practical limits

No live microphone/listening session or FL Studio routing session was performed. The drum test uses synthetic audio and does not establish real-world recognition accuracy. No claim of measured end-to-end latency or comparative CPU improvement is made. The separate Steinberg validator was not configured in pluginval. macOS was not built or tested. Windows binaries are a development build and are not digitally signed.

Original installed application and plugin were not overwritten. No GitHub repository was changed.

See the included regression and pluginval logs for the observed checks.
