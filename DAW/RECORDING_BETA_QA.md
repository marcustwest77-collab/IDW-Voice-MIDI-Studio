# IDW V14 Recording Beta — Hardware QA Gate

Status: **NOT RELEASE-CERTIFIED**. This checklist must be completed on real Windows and macOS systems with an audio interface. A GitHub Actions compile/test pass is not equivalent to hardware QA.

## Test setup
- Note OS/version, DAW commit SHA, interface make/model, driver version, sample rate, buffer size, and enabled input/output channels.
- Use closed-back headphones. Keep the monitor toggle OFF initially. Do not connect powered speaker outputs to line/mic inputs.
- Keep a backup of a test project and use disposable audio recordings. Confirm the recording folder has sufficient disk space.

## Required recording tests
- [ ] Mono voice recording: arm one empty audio track, select the physical microphone input, record at least 30 seconds, stop, verify WAV integrity and timeline import.
- [ ] Stereo/dual-input setup: arm two empty audio tracks on distinct inputs, capture both for at least three minutes, verify both WAV durations agree and that no dropped-block/write-overflow alert appears.
- [ ] Input protection: deliberately assign the same input to two armed tracks; confirm recording is refused without overwriting or losing existing files.
- [ ] Overdub: load a beat, arm an empty vocal track, enable PLAY BEAT WHILE RECORDING, capture a new vocal, then stop, listen and inspect alignment near the start and end of the song.
- [ ] Disable PLAY BEAT WHILE RECORDING and confirm independent recording remains possible.
- [ ] Input monitoring: enable HEADPHONES ONLY only during an eligible multi-input take, check quiet output level and safe toggling, stop recording and verify monitoring turns OFF.

## Loopback timing validation
- [ ] Use CREATE PULSE WAV to produce the five-pulse 48 kHz test file. Confirm the generated file plays without clipping.
- [ ] Route a supported *line-level* audio interface output back into a supported line input; record the returned reference while preserving the original WAV.
- [ ] Use ANALYZE LOOPBACK WAV and pulse times `100,200,300,400,500`. Record lag, jitter, accepted-pulse count and quality. Repeat at least three times without changing settings.
- [ ] Repeat after switching sample rate (44.1 kHz / 48 kHz) and audio buffer size (if available). Do not treat one calibration as valid across configurations.
- [ ] Compare beginning/end transient alignment of an overdub to identify drift as well as a fixed offset. Manual TAKE TRIM is non-destructive but not automatic calibration.

## Session and release gates
- [ ] Save a project with assigned inputs, reopen, verify assignments persisted and tracks are disarmed.
- [ ] Confirm existing WAV/AIFF tracks, MIDI notes, mixer, and export still work after overdubbing.
- [ ] Bounce full song to WAV, reopen bounced file and verify its duration, channels, audio continuity and audible content.
- [ ] Run a 30-minute recording and playback stress test with no crashes, corrupt recordings, missing files or unintended recording-monitor audio.
- [ ] Verify all checks on both Windows and macOS, noting any device-specific failures.

**Release blocker:** Any missing/corrupt recording, persistent drift, unsafe monitor feedback, or failed project recovery blocks the recording beta until resolved. Do not claim sample-accurate capture until a repeatable clock-aligned hardware test passes.
