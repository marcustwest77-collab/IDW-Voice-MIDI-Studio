# IDW Voice MIDI Studio

IDW Voice MIDI Studio is a clean-room voice-to-MIDI performance plugin and standalone application from In Da Wind Entertainment, paired with an optional Python companion for lyrics and take management.

## Formats
- Windows: Standalone + VST3
- macOS: Standalone + VST3 + Audio Unit

## Core Features
- Real-time YIN pitch detection
- Voice-to-MIDI note generation
- Vocal-level MIDI velocity
- Scale-aware pitch bend
- Custom scale lock and root selection
- Beatbox-to-drum MIDI triggering
- Gesture-to-MIDI CC
- MPE support
- MIDI Learn
- User preset save/load
- Noise-floor calibration
- Pitch calibration
- Retrospective voice-MIDI capture (recover the take you just performed, even if you forgot to hit record)
- Saved song scenes (recall sounds, harmony, scale and drum-note settings per song section)
- Voice profiles for multiple singers/ranges
- Built-in Studio Instrument: a 32-voice synth (Lead/Chords/Bass patches, synthesized drums on channel 10) that can play directly from the plugin's own MIDI output, so you can perform without loading a separate instrument

### Factory Presets
- Clean Vocal
- Tight Tracking
- Smooth Lead
- Scale Locked Lead
- Wide Bend Performance
- Beatbox Drums
- Expressive MPE
- Live Responsive

## User Experience
- Built-in **HELP / QUICK START** manual
- Hover tooltips for the main performance controls
- Factory preset browser
- Saved user presets appear in the same browser
- Full written manual in `docs/USER_MANUAL.md` and release packages

## Quick Start in FL Studio
1. Put IDW Voice MIDI Studio on the mixer insert receiving your microphone.
2. Set the IDW wrapper MIDI Output Port to a value such as `10`.
3. Set the destination synth wrapper MIDI Input Port to the same value.
4. Press **CALIBRATE NOISE** while the room is quiet.
5. Load **Clean Vocal**.
6. Sing and confirm the destination instrument follows the voice.

See `docs/USER_MANUAL.md`, `docs/FL_STUDIO_SETUP.md`, or press **HELP / QUICK START** inside the plugin for the full guide. `Integrations/FL-Studio/IDW-Performance/device_IDW_Performance.py` is an optional FL Studio controller script for routing.

## Audio Lab (Python companion)
`Companion/` is a separate, optional Python 3.10 application — it does not run inside the plugin and is not required to use the plugin.

- Offline lyric dictation and a distraction-free lyrics editor with autosave, previous-version recovery, named checkpoints (up to 100 per song) and plain-text import/export
- Non-destructive MIDI take editor with a piano-roll view, undo/redo, and take recovery/archiving
- Local, offline WAV-to-MIDI transcription (via Basic Pitch) for a recorded vocal take — not a live streaming feature
- Optional cloud voice conversion via an existing Kits.ai account and voice model

Run it with `Companion/Run-Audio-Lab.cmd` (Windows) after installing Python 3.10. Run `Companion/Setup-Transcription.cmd` once, with internet access, to add the optional local transcription model.

Run the companion's own test suite (no plugin build required):

    python -m unittest discover -s Tests -p "test_*.py" -v

## Build the plugin
The CMake project is pinned to JUCE `9.0.2` and supports an optional local checkout at `ThirdParty/JUCE`; otherwise CMake fetches JUCE automatically.

Windows:

    powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1

macOS:

    chmod +x scripts/build-macos.sh
    ./scripts/build-macos.sh

Linux (for running the C++ test suite; JUCE plugin formats on Linux are not part of the supported release matrix):

    chmod +x scripts/build-linux.sh
    ./scripts/build-linux.sh

## Continuous Integration
`.github/workflows/build.yml` runs on every push and pull request to `main`: it builds Windows and macOS Release binaries, runs the C++ regression suite, validates the VST3/AU with pluginval/auval, runs the full Python companion test suite, and packages installers as build artifacts.
