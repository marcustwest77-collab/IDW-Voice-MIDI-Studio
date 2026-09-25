# Repository review for Version 3

Reviewed from `marcustwest77-collab/IDW-Voice-MIDI-Studio` on 2026-09-22.

## Repository state

- Default branch: `main`
- Other branches: `help-presets`, `release-candidate-v8.2`, `v9-polish`
- Open/closed issues returned by repository issue search: none
- Main starting commit: `6b5902c81b56e1ab802adc48a5ddbc437ef86493`
- Starting product state: V9.1 help, user manual, factory presets, build workflow,
  Windows/macOS packaging scripts, and JUCE voice-to-MIDI source.

## Preserved functionality

- YIN monophonic pitch detection and vocal-level velocity
- Note output, pitch bend, scale lock, custom scale mask, and root selection
- Noise gate, pitch calibration, beatbox drum notes, gesture CC, MIDI Learn, MPE
- Factory/user presets, built-in quick start, VST3/Standalone formats, macOS AU
- Windows/macOS build and installer definitions

## Version 3 priorities selected

The repository has no issue backlog, so improvements were chosen from source and
manual review. The highest-impact feasible changes were reliable one-second noise
calibration, pitch smoothing, note-change confirmation, buffer-independent note
release, reduced gesture-CC traffic, and a detailed FL Studio connection guide.

## Validation status

- Git patch whitespace validation: passed.
- CMake configuration reached JUCE dependency setup using CMake 3.31.6.
- Native Linux configuration then stopped because this workspace lacks the
  required `pkg-config` executable/system audio development packages.
- Windows VST3/installer and macOS VST3/AU/package builds require their native CI
  runners or development machines. No GitHub workflow was triggered because the
  user prohibited GitHub writes without approval.
