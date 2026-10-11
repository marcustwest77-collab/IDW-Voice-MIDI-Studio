# IDW Voice MIDI Studio

IDW Voice MIDI Studio V13 (local beat-arrangement preview) is a clean-room voice-to-MIDI and vocal-performance application from In Da Wind Entertainment, paired with Audio Lab for offline vocal tuning, lyrics and take management.

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
- V13 adds custom drum patterns per bar, range copying, and beat undo/redo. See [beat arrangement and build instructions](docs/V13-BEAT-ARRANGEMENT.md). V13 native builds and installer validation are pending.
- Native V12.12 builds passed on Windows and macOS: 32/32 CTest tests on each platform, plugin checks, and installer generation. See [download and installation results](docs/V12.12-NATIVE-BUILD-RESULTS.md). Hardware and manual GUI testing remain pending.
- V12.12 adds a background full-song level check with track/mix peaks, silence detection and limiter-threshold timing. See [export checks](docs/V12.12-EXPORT-CHECK.md).
- V12.11 adds live track meters, beat/mix peak holds, pre-limiter warnings and peak reset in Timeline. See [mix meters](docs/V12.11-MIX-METERS.md).
- V12.10 adds background mix/stem export with progress, cancellation and a stable song snapshot. See [background export](docs/V12.10-BACKGROUND-EXPORT.md).
- V12.9 adds five aligned track WAVs for DAW transfer, with current mix settings and tempo/import instructions. See [stem export](docs/V12.9-STEM-EXPORT.md).
- V12.8 adds phrase selection: keep surrounding audio while choosing a replacement phrase, with shared sources and a single undo step. See [phrase selection](docs/V12.8-PHRASE-SELECTION.md).
- V12.7 adds independent clip mute and take selection with undo, recovery, saved choices and matching WAV export. See [take selection](docs/V12.7-TAKE-SELECTION.md).
- V12.6 adds Record cursor: record new clips into occupied armed tracks with count-in, preserved takes, clean capture and undo. See [recording clips](docs/V12.6-RECORD-CLIPS.md).
- V12.5 adds 1x–32x timeline zoom, horizontal scrolling, Play cursor and protected playback seeking. See [zoom and seeking](docs/V12.5-ZOOM-SEEK.md).
- V12.4 adds named bar-range markers and sample-clocked section playback/looping, with undo, persistence and recovery. See [sections and loops](docs/V12.4-SECTIONS-LOOP.md).
- V12.3 adds per-clip fades, linear overlap crossfades and musical-grid snapping. Fade edits support undo, recovery and WAV export. See [fades and snapping](docs/V12.3-FADES-GRID.md).
- V12.2 supports up to 64 clips on each of four audio tracks, independent WAV imports, nondestructive split/copy/move, track mixing, undo and embedded source sharing. See [the arrangement guide](docs/V12.2-CLIP-ARRANGEMENT.md).
- V12 four audio lanes with embedded WAV import, waveform overview, clip placement/trim, gain/pan/mute/solo and bounded track-edit undo/redo. V12.1 adds explicit recording arm for any of the four lanes, overwrite protection and recording undo. Other lanes play without being baked into the take. See [recording setup](docs/V12.1-TRACK-ARMING.md). See [the V12 guide](docs/V12-FOUR-TRACK-STUDIO.md).
- V11.4 recording count-in (Off/1/2 bars), adjustable metronome and bar/beat display. Clicks are excluded from the recorded track and WAV mix. See [the V11.4 guide](docs/V11.4-COUNT-IN.md).
- V11.3 background recovery: lyric/dictation writing checkpoints, stopped-song checkpoints, previous-copy fallback and confirmed restore. In-progress audio is not recovered. See [the V11.3 guide](docs/V11.3-SONG-RECOVERY.md).
- V11.2 native offline dictation: shared IDW microphone input, reviewable transcript and append-to-lyrics workflow. Requires a compatible Vosk runtime/model; see [the V11.2 guide](docs/V11.2-NATIVE-DICTATION.md).
- V11.1 native lyrics editor: song title, section headings, UTF-8 text import/export and lyrics saved with the beat and take. See [the V11.1 guide](docs/V11.1-UNIFIED-LYRICS.md).
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
