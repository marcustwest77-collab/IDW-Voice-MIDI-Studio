# IDW Voice MIDI Studio V10 — Vocal FX Edition

V10 adds an original, local pitch-correction workflow while preserving the V9.2 voice-to-MIDI, arrangement, built-in instrument, capture, lyrics and guided-routing features.

## What was added

- Real-time monophonic vocal pitch correction in the standalone, VST3 and AU builds.
- Chromatic targeting or song-scale targeting using the existing root and 12-note scale strip.
- Retune Speed, Tune Amount, Humanize, Wet Mix and Output controls.
- Six starting points: Natural, Smooth R&B, Tight, Memphis Hard, Singing Rap and Robot.
- Live target-note, correction and approximate algorithmic-latency readout.
- Non-destructive Audio Lab **Tune Studio** for local WAV processing and Pro Tools Intro compatibility.
- V10 parameter migration: old sessions and presets load with Vocal Tune disabled.

## Real-time setup

1. Use headphones to prevent microphone feedback.
2. Open the standalone or load the VST3/AU in a compatible host.
3. Confirm the microphone meter moves, then calibrate the noise gate while quiet.
4. Open **Vocal FX** and enable **Vocal Tune**.
5. Start with **Natural**. For a harder effect, choose **Tight**, **Memphis Hard** or **Robot**.
6. For song-scale tuning, choose the root and enabled notes in Studio controls, then select **Song scale** in Vocal FX.

The live tuner outputs corrected microphone audio automatically when enabled. The ordinary **Hear microphone** switch remains for untuned monitoring when Vocal Tune is off.

## Pro Tools Intro on Windows

Pro Tools uses AAX and does not load the included VST3. V10 therefore provides a non-destructive file workflow:

1. Record a dry vocal through the AudioBox USB 96. Do not print reverb or delay.
2. In Pro Tools, consolidate the vocal from the exact session start (or another clearly recorded sync point).
3. Export it as 16-bit PCM mono WAV at the same sample rate as the session, normally 48 kHz.
4. Open **IDW Audio Lab > Tune Studio** and choose that WAV.
5. Select root, scale and a preset. Use Natural first; hard settings are intentional special effects.
6. Click **Create corrected WAV copy**. Tune Studio refuses to overwrite the source.
7. Import the new `-IDW-Tuned.wav` into Pro Tools on a new audio track at the same start time.
8. Mute the original to audition the corrected track. Keep the original for edits and backup.

## Practical starting settings

| Sound | Retune | Amount | Humanize | Mode |
|---|---:|---:|---:|---|
| Natural | 95 ms | 72% | 72% | Song scale |
| Smooth R&B | 70 ms | 82% | 62% | Song scale |
| Tight | 25 ms | 95% | 22% | Chromatic |
| Memphis Hard | 8 ms | 100% | 3% | Song scale |
| Singing Rap | 18 ms | 96% | 15% | Song scale |
| Robot | 5 ms | 100% | 0% | Chromatic |

## Current limits

- The tuner is monophonic: process one dry lead vocal at a time.
- Formant preservation is not yet included, so large corrections may change vocal character.
- V10 does not include AAX. Use Tune Studio for vocal audio and the standalone/loopMIDI workflow for voice-to-MIDI in Pro Tools Intro.
- This is an original IDW implementation, not a copy of Voloco, Auto-Tune or another product.
