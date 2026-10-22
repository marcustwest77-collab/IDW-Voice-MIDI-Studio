# V11 Song Studio — first local preview

V11 adds a native **Song Studio** panel to the existing IDW app. This is the first beat-plus-recording workflow, not a finished multitrack DAW.

## What works in this candidate

- Three synthesized drum lanes: kick, snare and closed hi-hat.
- A 16-step sixteenth-note pattern repeated in 4/4 time.
- 40–240 BPM and 1–64 bars, limited to a maximum of 180 seconds.
- Play, Record take, Stop and explicitly confirmed Discard take.
- One stereo audio performance track beginning at bar 1. It records IDW's processed vocal/instrument output, excluding Song Studio's beat. MIDI notes are baked into audio if the Studio Instrument is enabled.
- Separate beat and recorded-track levels, editable while stopped.
- A self-contained `.idwsong` file containing the beat, tempo, length, levels, sample rate and recorded audio. No external audio paths are required.
- 48 kHz, 24-bit stereo WAV mix export. Export renders the chosen bar count and truncates audio/tails beyond that boundary.

## First session — Windows / AudioBox USB 96

1. Launch the IDW **standalone application** and choose the working AudioBox audio driver and outputs in Audio Settings. Connect headphones to the interface.
2. Select the microphone input in IDW (Left/mono for input 1; Right for input 2). Verify the input meter. Avoid red/clipping input levels.
3. Choose whether to record vocals or the voice-driven instrument. For vocals only, disable Preview sound and Studio Instrument. If you want the generated instrument recorded too, enable Studio Instrument; its sound is recorded together with the voice. These cannot be separated after recording.
4. Set any Vocal FX you want printed into the take. Recording is a processed take, not a separate dry microphone stem.
5. Click **Song Studio** beneath the title. Click numbered steps to add/remove drum hits. Set BPM and Bars. Play song to hear the beat, then Stop.
6. Click **Record take**. Recording begins immediately at bar 1; this preview has no count-in. It enables microphone monitoring. Use headphones and avoid duplicate interface direct monitoring if you hear an echo.
7. Click **Stop**, or let the chosen song length end the take. Record take refuses to overwrite an existing take. Save it before using Discard take.
8. Click **Save song**, name the `.idwsong` file and keep it with your music projects. The recording is in memory until saved. This preview has no crash recovery or automatic save-on-close.
9. Click **Play song** to hear the saved-in-memory take and beat. During Song Studio playback, the live input/instrument output is replaced by the song mix.
10. Click **Export mix WAV** to make the stereo song file. This file can be played elsewhere or imported into Pro Tools.

Song Studio uses its own transport and tempo; it is not synchronized to a DAW host transport. Closing the panel does not stop playback; use Stop or PANIC. Stop does not turn off the existing microphone monitor switch. Song files are separate from DAW plugin state/presets. Save a `.idwsong` explicitly before closing or switching projects.

Changing BPM changes the drum timing but does not time-stretch the recorded take. Reducing Bars shortens playback/export without deleting the take from the song file. WAV export may briefly occupy the UI while the transport is stopped. Recording allocates memory in advance; at 48 kHz a 180-second stereo take uses about 69 MB.

## Build and tests

Use the repository's existing CMake build process. New CTest targets:

- `IDWSongPattern`: sample-clock boundaries, fractional tempos and duration limits.
- `IDWSongStudio`: real JUCE-linked record/play, mono-to-stereo capture, project round trip, truncated-file rejection, sample-rate conversion, drum audio and WAV export.

Locally, both targets were compiled and executed independently. Editor/processor source is also syntax-checked against the local JUCE headers. These checks do not substitute for a full Windows/macOS build, plugin validation, visual testing or real microphone testing.

Manual acceptance before release:

- Install and launch on Windows 10 with AudioBox USB 96.
- Inspect Song Studio at minimum size and with all built-in/custom skins.
- Record a vocal-only take and a combined vocal/instrument take; listen for dropouts or channel imbalance.
- Compare playback with exported WAV and confirm timing, levels and intended end point.
- Save/reopen, try a read-only destination, and cancel every file/confirmation dialog.
- Stop/PANIC during recording and confirm partial-take preservation.
- Change audio devices/sample rates and check playback of an existing take.
- Confirm work survives a normal close only after an explicit Save song.

## Following milestones

Count-in/metronome, waveform timeline, clip placement and trimming, multiple takes/tracks, MIDI arrangement editing, imported samples, undo/redo and crash-recovery recording remain future work. No third-party plugin hosting is included. V10.7 custom skins are included in this local branch but have not yet been built by GitHub CI.
