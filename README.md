# IDW Voice MIDI Studio

IDW Voice MIDI Studio V11 (local Song Studio preview) is a clean-room voice-to-MIDI and vocal-performance application from In Da Wind Entertainment, paired with Audio Lab for offline vocal tuning, lyrics and take management.

The official IDW Entertainment crown, wings and microphone logo is embedded in the interface, standalone startup screen and platform packaging. Source and deployment variants are kept in `Resources/Brand`.

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
- Real-time monophonic vocal pitch correction with chromatic/song-scale targets, retune speed, correction amount, humanize, wet mix and output controls
- Live vocal production rack with individually bypassable de-esser, compressor, saturation, stereo doubler, reverb and delay plus a master wet/dry mix
- Local Auto-Key Assistant that learns from a sung phrase, suggests root plus major/minor mode with confidence, and applies it to song-scale tuning only after confirmation
- Adaptive Tune with controllable vibrato preservation: faster note transitions and gentler sustained-note correction
- Slow Level Match for fairer rack bypass comparisons and BPM-synchronized 1/8, dotted-1/8, 1/4 and 1/2 delay divisions
- Optional LPC-based Formant Preserve (Beta) path for reducing chipmunk/boomy artifacts during larger correction moves
- Scale-aware audio harmonies with upper-third, low/high-third, third/fifth and octave-stack voicings
- V10.5 Clip Guard, confidence-gated harmonies, Eco/Studio/High CPU modes, Safe Tracking and temporary A/B Vocal FX snapshots
- V11 Song Studio: native 16-step drum sequencer, one recorded audio track, self-contained song files and stereo WAV export (local preview; see docs/V11-SONG-STUDIO.md)
- V10.7 Custom Skin Designer with five editable RGB colours and reusable named local skins
- V10.6 Custom Skin Studio with six live-switchable, project-persistent interface skins
- Atomic user-preset saving with one-copy recovery fallback
- Local, non-destructive Tune Studio for creating corrected WAV copies that can be imported into Pro Tools Intro

### Factory Presets
- Clean Vocal
- Tight Tracking
- Smooth Lead
- Scale Locked Lead
- Wide Bend Performance
- Beatbox Drums
- Expressive MPE
- Live Responsive
- Natural Vocal Tune
- Smooth R&B Tune
- Tight Vocal Tune
- Memphis Hard Tune
- Singing Rap
- Robot Voice
- Safe Tracking
- Wide Pop Harmony
- Low CPU Live
- Modern Rap Lead

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

## Quick Start in Pro Tools Intro on Windows
Pro Tools does not load the included VST3, so use the IDW **standalone** application with a virtual MIDI port:
1. Create an `IDW Voice MIDI` port in loopMIDI.
2. In IDW **Options > Audio/MIDI Settings**, select the AudioBox USB 96 input and output plus `IDW Voice MIDI` as the MIDI output.
3. In Pro Tools, enable `IDW Voice MIDI` under **Setup > MIDI > Input Devices**.
4. Create and record-arm a stereo Instrument track, load an instrument, and select `IDW Voice MIDI` channel 1 as its MIDI input.
5. Open IDW **Setup / Help**, choose **Pro Tools Intro / Windows**, mute IDW sounds, and press **Send test note**.

See `docs/PRO_TOOLS_SETUP.md` for the complete AudioBox USB 96 checklist and troubleshooting flow.

### Correct vocals for Pro Tools Intro

Pro Tools Intro cannot load the included VST3. For vocal audio, use **Audio Lab > Tune Studio**:

1. In Pro Tools, consolidate the dry mono vocal from the song start and export it as 16-bit PCM WAV at the session sample rate.
2. Open IDW Audio Lab, choose the WAV, and select **Tune Studio**.
3. Start with **Natural** or **Smooth R&B**. Select the song root/scale when you want scale-safe correction.
4. Create a corrected WAV copy. The source file is never overwritten and local tuning does not upload audio.
5. Drag the `-IDW-Tuned.wav` copy onto a new Pro Tools audio track at the same song start. Mute the dry track to compare; keep it for backup.

## Audio Lab (Python companion)
`Companion/` is a separate, optional Python 3.10 application — it does not run inside the plugin and is not required to use the plugin.

- Offline lyric dictation and a distraction-free lyrics editor with autosave, previous-version recovery, named checkpoints (up to 100 per song) and plain-text import/export
- Non-destructive MIDI take editor with a piano-roll view, undo/redo, and take recovery/archiving
- Local, offline WAV-to-MIDI transcription (via Basic Pitch) for a recorded vocal take — not a live streaming feature
- Local Tune Studio for monophonic pitch correction and Pro Tools-compatible WAV export
- Optional cloud voice conversion via an existing Kits.ai account and voice model

The V10 Windows installer includes a self-contained **IDW Audio Lab** Start-menu shortcut with its offline models. Source users can instead run `Companion/Run-Audio-Lab.cmd` after installing Python 3.10, then run `Companion/Setup-Transcription.cmd` once to add the optional local transcription model.

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
