# IDW Voice MIDI Studio — User Manual

IDW Voice MIDI Studio turns a live singing or beatboxing voice into MIDI in real time. It ships as a native C++/JUCE plugin (Standalone, VST3, and on macOS also Audio Unit) plus an optional Python companion, Audio Lab, for lyrics, dictation and take editing. This manual covers the plugin; see [Audio Lab](#audio-lab-companion) below for the companion app, and `docs/FL_STUDIO_SETUP.md` for FL Studio-specific routing. It documents version 9.2.

## The editor at a glance

- Two screens, switched by the button in the top right: **Performance view** (large pitch readout, status line, harmony, voice profiles, voice range and the record / export / recover row) and **Studio controls** (gate, confidence, bend and tune knobs, the scale strip, drum pads and MIDI Learn). Both keep the logo header, and the **Studio instrument** and **Setup / Help** panels open over whichever screen is showing.
- The header strip carries the IDW logo, centred above the main pitch readout and held in the same place on every screen. The artwork is drawn on black, so its padding is keyed to transparency with a luminance ramp when it is prepared (`scripts/key-logo-alpha.py`): the glow fades into the panel instead of sitting in a dark rectangle, and the whole outer edge of the logo tile is fully transparent, so it leaves no plate and no hairline against the panel.
- The editor opens at 1120x932 and can be resized down to 1040x922; the logo stays centred and its tile edge stays clear at both sizes.

## First sound

1. In the standalone, open **Options > Audio/MIDI Settings** and choose your microphone and headphone/speaker output. In a DAW, load IDW on the mixer channel/insert receiving your microphone instead.
2. Enable **Preview sound**, then press **Test note** — you should hear a short C4 tone and see MIDI activity.
3. Stay quiet and press **Calibrate noise**, then sing a steady note. The pitch/note readout should track your voice.
4. Turn **Preview sound** off once you've confirmed signal — it's a simple built-in check tone, not meant to run alongside your real instrument.
5. Load a factory preset (see below) as a starting point instead of tuning every control by hand.

## Core performance controls

- **Noise gate** — minimum input level treated as a real note (default ~8 ms attack window). Raise it in noisy rooms; lower it for very quiet, close-mic'd sources.
- **Confidence** — how certain the pitch detector must be before it emits a note (0.5–1.0, default 0.75). Higher values reject more ambiguous/breathy input.
- **Pitch calibration** — a ±100-cent fine-tune trim if your reference pitch is slightly off A440.
- **Bend range** — semitone range for the pitch-bend messages IDW sends (1–24 semitones, default 2). Match this to your destination instrument's own bend range.
- **Voice range (Lowest/Highest Voice Note)** — restricts note generation to the octave range you actually sing in.
- **Scale Lock and the 12-key scale strip** — enable Scale Lock, pick a root note, then toggle the twelve buttons (labelled relative to that root) to build a custom scale. Notes outside the lit keys are pulled to the nearest allowed note.
- **Scale Expression** — when Scale Lock is on: **Strict scale** locks both the note and its sounding pitch to the selected scale; **Natural vibrato** keeps up to ~45 cents of your original pitch deviation around the scale note, for a more human, less quantized feel.
- **Melody Tracking** — the master on/off for monophonic voice-to-MIDI note generation.

## Harmony

Enable harmony to generate extra MIDI notes that turn your lead line into a chord:
- **Harmony Scale** — Harmony off, Major scale or Natural minor.
- **Chord Voicing** — triads or sevenths.
- **Bass Layer** — adds a bass-register doubling of the chord root.
- Lead notes go out on MIDI channel 1, harmony on channel 2 and the bass layer on channel 3, so a multitimbral instrument can give each part its own sound. Harmony follows the nearest scale degree (ties take the lower note); your bend and vibrato stay on the lead.
- MPE suppresses the chord and bass layers, since its member channels would collide with them.

## Beatbox drums

Enable **Beatbox** to trigger MIDI drum notes from vocal percussion instead of pitched notes:
- **Beat Threshold** sets the onset sensitivity.
- Kick, snare and hat each map to a configurable MIDI note (defaults 36/38/42, General MIDI-compatible), plus five additional drum pads for a fuller kit.
- Beatbox notes are sent on MIDI channel 10 alongside melody on channel 1, so both can go to the same destination track if it separates by channel.
- **Drum Lab** — with nothing trained, the engine uses basic kick/snare/hat spectral rules. Select a pad, press **Train 5 hits**, and perform five distinct examples with pauses between them; melody and drum output are muted while it learns. Once any pad has a profile, only trained pads are classified and unfamiliar hits are rejected. **Reset pads** clears every profile and restores the basic three-sound detection. These are local spectral templates, not a neural model, so accuracy depends on your microphone, background noise and how distinct the sounds are — and they are saved with your user presets.

## Gesture MIDI CC and MPE

- **Gesture CC** turns expressive vocal movement into a continuous MIDI CC message (default CC74) with adjustable sensitivity — useful for driving a filter or expression parameter on your destination synth.
- **MPE** sends each active note on its own MIDI channel across a configurable channel zone (default channels 2–16), for MPE-compatible synths. Harmony is disabled while MPE is active, since MPE is inherently monophonic per channel, and drum notes stay on channel 10, which the MPE zone leaves free.

## MIDI Learn

Map any of the main performance parameters (noise gate, confidence, bend range, beat threshold, gesture sensitivity, scale expression) to an external MIDI controller: pick the target, engage learn mode, then move a knob/fader on your controller.

## The built-in Studio Instrument

IDW includes a compact 32-voice synthesizer so you can perform without loading a separate instrument:
- Independent **Lead**, **Chords** and **Bass** parts, each choosing from five patches (Sub/sine, Saw lead, Pulse, Warm keys, FM bell).
- Volume, brightness, attack, release and a feedback echo (up to ~600 ms).
- Drums are synthesized on channel 10 alongside the pitched parts.
- Enabling the Studio Instrument automatically stops microphone monitoring and the simple preview, so you always know which sound path is live.
- It's an original compact synth, not a sample library, voice clone, or emulation of any commercial instrument.

## Presets

- Factory presets: Clean Vocal, Tight Tracking, Smooth Lead, Scale Locked Lead, Wide Bend Performance, Beatbox Drums, Expressive MPE, Live Responsive.
- Save your own settings as a user preset; it appears in the same browser as the factory presets.
- Loading any preset is a good moment to recalibrate noise if your room or microphone has changed.

## Retrospective capture (never lose a take again)

**Remember voice MIDI** (on by default) continuously retains up to the last 30 seconds of MIDI that IDW generated — lead, harmony, bass and beatbox — while audio is processing. If you play or sing something great without recording, press **Save last 30s** to freeze that buffer and export it as a `.mid` file.

Notes on how it behaves:
- It captures MIDI IDW generated, not microphone audio or incoming keyboard MIDI, and it does not touch your DAW's own recorded take.
- The buffer follows audio-processing time, not wall-clock time; it stops advancing if your DAW stops processing.
- It's a creative recovery aid with a bounded internal buffer, not a crash-proof recorder — extremely dense MIDI can shorten the effective window, and a stalled host can cause a gap (flagged with a warning) rather than silent data loss.

## Recording and exporting a take

- **Record MIDI** captures what you perform; **Stop recording** closes the take. The take belongs to the plugin rather than the editor window, so closing the editor does not end it, and starting a new take first preserves the previous one.
- A take is limited to ten minutes or 250,000 events, and overflow is shown in the status line. Set **BPM** to your DAW tempo before recording — tempo is fixed for each take.
- **Export MIDI** writes notes, drums, bends and controller expression to a `.mid` file you can drag into your DAW. Capture starts from new MIDI events, so begin recording before you sing; notes still held when you stop are closed in the exported file without cutting your live instrument.
- **Recover take** opens the Recovered Takes folder. Backups are checkpointed roughly every five seconds while the host stays responsive, and at stop/close, so a crash costs at most the latest checkpoint: pick a backup and export it. If a backup cannot be written, export manually.

## Song scenes

Save a named scene (e.g. "Verse", "Hook", "Bridge") to recall your Studio Instrument sound, harmony/scale, bend range, and beatbox/drum-note settings in one click. Scenes deliberately do **not** touch microphone calibration, voice range, tempo, or MIDI routing — those stay as you last set them so switching scenes mid-song doesn't undo your calibration. Scenes are stored as local XML files under `IDW Voice MIDI Studio/Song Scenes` in your user application-data folder; saving over an existing name replaces it atomically.

## Voice profiles

Save and recall calibration/voice-range settings per singer, isolated from the musical settings (harmony, scale, instrument) so multiple vocalists can share one project without re-calibrating each time. **Save voice** stores the input mode, noise gate, confidence, pitch calibration and low/high voice limits, never the musical settings. **Learn range** listens for 12 seconds while you sing your comfortable low and high notes and then fills in the limits; **Cancel range** keeps the existing range. Reversed limits are swapped automatically. Profiles do not choose the microphone device, so keep one named profile per microphone.

## Setup / Help panel

Open **Setup / Help** for a live diagnostics view: whether audio is processing, current microphone level, MIDI generated since the panel opened, and which sound path (instrument, monitor, or DAW-only) is currently enabled. It also has **Send test note** (MIDI note 60, channel 1, only available while audio is running) and a **Copy setup report** button useful when asking for support — the report contains signal levels and settings, never microphone recordings or account credentials.

## FL Studio and other DAWs

1. Put IDW on the mixer insert receiving your microphone.
2. Set IDW's MIDI Output Port (in the plugin wrapper settings) to an unused port such as 10.
3. Set your destination instrument's MIDI Input Port to the same number.
4. Calibrate noise while the room is quiet, load a preset such as Clean Vocal, and sing.

In the standalone there is no built-in virtual MIDI driver: create a virtual port (loopMIDI on Windows works well), enable that port as an input in FL Studio, set the receiving instrument's Input port to match, then select the same port as the standalone's MIDI output.

Full DAW-specific routing steps (including Pro Tools via loopMIDI) are in `docs/FL_STUDIO_SETUP.md`. `Integrations/FL-Studio/IDW-Performance/device_IDW_Performance.py` is an optional FL Studio controller script for routing.

## Audio Lab companion

`Companion/` is a separate, optional Python 3.10 application launched with `Companion/Run-Audio-Lab.cmd`. It does not run inside the plugin and nothing recorded or typed there leaves your machine.

**Lyrics and dictation**
- A distraction-free lyrics editor with autosave, section-heading shortcuts (Intro/Verse/Hook/Bridge/Outro), and Undo/Redo.
- Local, offline English dictation with a live microphone meter showing dBFS, plus clear "Quiet: check mic/gain" and "Clipping: lower input gain" hints, and Input 1/Input 2 selection for two-input audio interfaces.
- Optional spoken commands (off by default) for punctuation, new lines/paragraphs, and section headings while dictating.
- Song search across saved titles and lyric contents.
- Rolling history (last 20 saves) with one-click recovery as a separate song, plus named checkpoints (up to 100 per song) for milestones like "Original hook" or "Before rewrite."
- Import existing lyrics from a plain-text `.txt` file, or export the current draft to `.txt`.

**MIDI take editor**
- Open a recorded/exported `.mid` file (or jump straight to one you just transcribed) in a piano-roll view.
- Edit a note's pitch, velocity, start and length; delete notes; transpose a track (drums on channel 10 are skipped automatically); snap note starts to a grid (1/4 down to 1/32).
- Undo/Redo retain the last 20 edits. **Export copy** always writes a new file — your original take is never overwritten.

**Offline transcription**
- Convert a recorded 16-bit PCM WAV (mono/stereo, 8–96 kHz, up to 10 minutes/25 MiB) into MIDI locally using Basic Pitch, with adjustable onset/sustain thresholds and minimum note length. This processes a recorded file, not a live microphone stream, and works best with a single instrument or isolated stems.

**Optional cloud voice conversion**
- Available if you already have a Kits.ai account and voice model; this is the one Audio Lab feature that leaves your machine, and it's entirely opt-in.

Run the companion's own automated test suite anytime with:

    python -m unittest discover -s Tests -p "test_*.py" -v

## Troubleshooting

| Symptom | Check |
|---|---|
| No sound / no MIDI | Confirm the right mic is selected, gate/confidence aren't set too strict, and Melody Tracking is on |
| Destination instrument silent in a DAW | Confirm matching MIDI Output/Input port numbers, and that the receiving track is armed/monitoring |
| Noisy or false triggers | Recalibrate noise in a quiet room; raise the noise gate or confidence |
| Beatbox not triggering | Lower the beat threshold; confirm Beatbox is enabled |
| Dictation reports a device error | Check the selected input channel, Windows microphone permission, and whether another app is holding the interface |

## Where your data lives

- Song scenes: `IDW Voice MIDI Studio/Song Scenes` (user application-data folder).
- Lyrics, history, and checkpoints: `IDW Audio Lab/Lyrics` (user home folder).
- Setup reports and MIDI Learn mappings never leave your machine unless you explicitly export or share them.
