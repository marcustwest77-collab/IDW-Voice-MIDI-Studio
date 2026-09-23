# IDW Voice MIDI Studio V5 — arrangement and recovery candidate

This source candidate builds on V4.1. No V5 EXE has been compiled yet.

Open START-HERE.html for the visual setup guide. Run BUILD-WINDOWS.cmd with VS2022 Desktop development with C++, CMake, Windows SDK and Git installed to produce a tested local build. The branch-scoped windows-v5.yml workflow is prepared for GitHub but requires approval before upload/run.

## New in V5
- Diatonic major/natural-minor triads or sevenths from one detected voice note, with optional bass.
- Lead/channel 1, chord/channel 2, bass/channel 3, drums/channel 10. MPE suppresses harmony layers to prevent collisions.
- Named voice profiles: input channel, noise gate, confidence, tuning and comfortable note range. A 12-second range learner helps set the limits.
- Processor-owned takes continue when the editor is closed; periodic MIDI recovery files and recovery-file picker.
- Performance screen plus the full Studio controls, using IDW's dark/gold visual style.
- Explicit default migration for older V4 states/presets.

Read docs/V5-VALIDATION.md for checks performed versus pending. No latency reduction, polyphonic transcription, microphone auto-selection or additional synthesizer is claimed.

Plugin identity and old parameter IDs are retained. Test on a copy of a DAW session before replacing the existing V4.1 plugin. Keep only one version in scanned plugin folders.
