# Record, edit, save, reopen, export acceptance

## Automated engine check

Build `IDWDAWRecordEditRoundtripTests` and run
`ctest --test-dir build-v14 -C Release -R '^IDWDAWRecordEditRoundtrip$' --output-on-failure`.
The Windows/macOS foundation workflow builds and runs this test.

This exercises production engines with synthetic callback samples; it does not
open a physical microphone or drive the Workstation UI. It records two seconds
of stereo audio from input channels 1/2 while excluding a distinct channel 0,
then splits, trims, moves, adjusts gain/pan/fades, undoes/redoes, saves, reopens
into a fresh session, reads a recovery checkpoint, and exports stereo WAVs at
48 kHz and 44.1 kHz. Assertions cover frame counts, channel routing, persisted
metadata, timeline silence/gaps, source offsets, gain/pan, fade endpoints,
resampling, source-file preservation, and refusal to overwrite an existing WAV.

An optional single argument names a **new** evidence directory. This retains the
synthetic source, `Workflow.idwproj`, recovery copy, and both exported WAVs for
inspection. Without an argument the test cleans up its own temporary directory.
The generated recording is synthetic test audio, not microphone evidence.

## Physical Windows acceptance (pending)

Use a separate current CI application and a new disposable project. Keep the
installed beta and important recordings unchanged. Use headphones and leave dry
monitoring off initially.

1. Open **AUDIO DEVICES**. Select the PreSonus AudioBox input/output and the
   microphone's physical input. Select an empty audio track, choose its input,
   enable **ARM INPUT**, then run **PREFLIGHT**. Record the device, rate, buffer
   size, input, and result. A configuration PASS does not prove microphone audio.
2. Press **RECORD VOCAL**, speak a distinctive phrase for 5–10 seconds, then
   **STOP**. Confirm the take's waveform/duration and play it back through
   headphones. Reject silence, clipping, unexpected channels, or dropouts.
3. Stop playback. Select the clip and perform a split/trim/move edit. Check undo
   and redo, then play back the edit and verify the expected gap/source segment.
4. **SAVE PROJECT** to a new `.idwproj`. Reopen it through **OPEN / RECOVER**;
   confirm tempo, routing, mixer settings, clip edits, and audible playback.
   Reopening must not automatically arm recording.
5. **BOUNCE WAV** to a new filename. Open the exported WAV independently and
   verify stereo format, duration, edited timing, signal level, and audibility.
6. Retain the project, original take, export, and observations. Mark each step
   PASS/FAIL with evidence; do not infer hardware success from engine tests.

## Initial run: 2026-11-01

- CI build tested: PR #209, commit `123e72d5114bfab2b1de35f7bfcf21058fa4ca0d`.
- Windows artifact ZIP SHA-256:
  `333c76e501841e9e5f9057415eeaf9416fb4ab48fe914a00a325b75593c315db`.
- Executable SHA-256:
  `30891dfc8f8788d0e6ae1981ce5d99ff8b919b8a03932656629b5401d1a9fbe7`.
- Separate application launches and exposes its controls through accessibility.
  Its initial device reports LG TV (Intel Display Audio), with Audio 1 disarmed.
- Screen capture failed with `FrameArrived timed out` and then
  `window capture timed out`; clicking failed with
  `coordinate input geometry is unavailable`. Keyboard attempts produced no
  verified state change. This is an automation limitation, not a proven DAW bug.
- User selected PreSonus AudioBox and reported a preflight PASS after manual
  routing/arming. Live microphone capture/playback and edit/reopen/export
  verification remain pending; preflight alone is not audio evidence.
- The production-engine synthetic roundtrip passes locally, retaining inspectable
  audio/project evidence. Installed beta files were not replaced.
- Existing recording-interruption, project-persistence, and stereo-bounce
  regression executables also pass locally. These checks do not establish
  physical AudioBox latency, monitoring quality, or full UI acceptance.

## AudioBox playback follow-up

The user subsequently clarified that playback had been silent throughout;
the earlier audible-playback response is superseded. Moving meters and a
waveform are not proof of audible output. Windows' own AudioBox test is audible,
but the DAW device-selector test remains silent even with input disabled.
The app volume/output assignment was checked by the user. Accessibility confirms
Windows Audio, Speakers (AudioBox USB 96), outputs 1/2 enabled, and 44.1 kHz.
The exact cause of the Windows Audio silence remains unproven.

The installed registry contains an AudioBox ASIO Driver, but prior DAW builds
omitted JUCE's ASIO backend. Windows DAW builds now enable `JUCE_ASIO=1`, using
the headers bundled with the pinned JUCE release. Windows Audio remains available;
this change neither installs a driver nor automatically changes the selected device.
A regression verifies backend factory availability using the same CMake
configuration as the workstation, without opening physical devices in CI.

In the separate updated Windows build, select **ASIO** and **AudioBox ASIO Driver**
in **AUDIO DEVICES**, enable its stereo outputs, and try **Test**. Hardware playback
remains pending until the user actually hears that tone and their recorded take.
See [JUCE 9.0.2 backend configuration](https://github.com/juce-framework/JUCE/blob/9.0.2/modules/juce_audio_devices/juce_audio_devices.h).

DirectSound subsequently produced the test tone, but the user also reported a
whining sound that continued after the test ended. Listening acceptance is
therefore failed, not passed. Save the test project and close that old build;
use low headphone levels when validating a corrected build.

Code inspection identified an independent, reproducible output-contract defect:
the workstation registered capture engines directly with AudioDeviceManager.
These engines may return without writing output, or add monitor/pulse samples to
existing output. JUCE mixes each registered callback's scratch buffer without
clearing it between secondary callbacks. The workstation now registers an
`IDWIsolatedAudioCallback` for each capture engine: it clears that callback's
output before delegating, while JUCE still mixes the independent playback stream.
Lifecycle callbacks are forwarded and registration/removal use the same wrappers.
This removes the stale-output path; whether it accounts for all reported hardware
noise/silence still requires the physical retest.

`IDWDAWAudioCallbackIsolationTests` feeds dirty scratch through the real single,
multitrack, and loopback capture engines. It verifies repeated idle silence,
silence during unmonitored capture and after interruption, isolated monitoring,
bounded calibration pulses and silence after completion, plus input-only/null
output handling. No hardware is opened. See
[JUCE's callback summation](https://github.com/juce-framework/JUCE/blob/9.0.2/modules/juce_audio_devices/audio_io/juce_AudioDeviceManager.cpp).

## Corrected-build physical acceptance: 2026-11-01

The preceding failed/pending observations describe the older build. On the
isolated PR #211 build (`e7b9ab0a19853631b88719124ff55b2ac4d9a6ab`), the user
selected AudioBox USB 96 ASIO with headphones and subsequently confirmed:

- Clean device test and audible recorded voice, without continuing whine.
- Split/move editing and undo/redo work.
- Save/reopen retains edits and correct playback.
- Exported WAV sounds correct in a separate audio player.
- The longer recording plays correctly (user report). A read-only header check
  of the user-provided `IDW Session9.idwproj` identified `Vocal Take36.wav`:
  16,475,104 mono frames at 44.1 kHz, 24-bit, or 373.585 seconds (6:13.585).
  The project preserves this source as four split clips alongside the earlier take.

This supersedes the earlier duration-unverified result from inspecting shorter
takes. It verifies a recording beyond five minutes and user-reported clean playback
on one setup. Hardware dropout counters, latency and sample continuity were not
measured by the agent. It is not multi-hour stability or other-device qualification.
The installed beta was not replaced. All five PR #211 CI checks passed,
including all 48 Windows workstation CTest cases.

## Extended recording engine regression

`IDWDAWExtendedRecordingTests` records deterministic stereo samples through the
production recorder and isolated callback at 48 kHz and 44.1 kHz. It streams the
finished WAVs back in bounded blocks and checks every sample and exact frame
count. A distinct unselected input must not leak into the take. Dirty output
scratch must remain silent. Recorder diagnostics must report no dropped/rejected
blocks, missing inputs or interruption; a subsequent odd-sized take checks clean
restart, counter reset and final-block flush.

A separate worker performs 12 snapshot save/reopen/recovery/export cycles per
rate while the recorder's disk writer remains open. Checkpoint revisions and
bounded recovery slots are verified; independent exported snapshots must have
the expected duration and stereo signal. This applies engine-level disk load;
it does not enable simultaneous recording and export/save in the Workstation UI.

CI runs a wall-clock-paced ten-second **sample timeline per rate**, with a
120-second test timeout. Run a larger generated timeline locally with:

```
IDWDAWExtendedRecordingTests --seconds 90 --evidence NEW-EMPTY-OUTPUT-DIRECTORY
```

The evidence directory must not exist yet. It retains both source takes,
snapshot projects, recovery copies, independent exports and restarted takes.
Without `--evidence`, the test cleans up its own temporary directory. Timeline
duration is paced with a steady clock (90 seconds per rate takes at least three
minutes total), but callbacks are synthetic. Early accelerated producer profiles
overran the writer's bounded queue; those are throughput-overload observations,
not physical dropout evidence. The clock-paced profile respects the production
writer's time budget. It does not measure physical latency, actual hardware
callback deadlines, or ASIO reliability.

Local clock-paced validation completed 90 seconds at each rate in 180.616 wall
seconds. All 4,320,000 stereo frames at 48 kHz and 3,969,000 at 44.1 kHz matched
the generated samples. Both runs reported zero dropped/rejected blocks, missing
inputs and interruptions. All 24 snapshot/recovery/export cycles and both odd-sized
recorder restarts passed. Generated evidence was retained in an isolated workspace
folder; no user audio was modified or uploaded.

## Simultaneous capture and software overdub regression

`IDWDAWMultitrackOverdubTests` captures two mono takes at 44.1 kHz and eight
at 48 kHz, using reversed input assignments and distinct generated samples.
Both cases run two seconds of clock-paced callbacks while the production WAV
reader/transport and `readClipWindow` play a backing file. Monitoring stays off;
the isolated capture callback must neither erase, double nor contaminate playback.
The two-take case also includes an unselected input with a distinct signal.

Every sample and exact duration of every captured mono file is verified. The
test saves/reopens a project containing the backing and takes, with take input
assignments, explicit half-second software placement, gain and hard pans. A
stereo WAV bounce is checked sample by sample, including backing-only lead/tail,
take offsets and channel separation. The original backing WAV must remain
byte-identical. CI builds/runs this test on Windows/macOS with a 60-second timeout.

Run `IDWDAWMultitrackOverdubTests NEW-EVIDENCE-DIRECTORY` to retain generated
takes, projects and mixes; the directory must not exist. Without an argument,
the test cleans up its own temporary output. This is a short device-free engine
integration check, not physical eight-input recording or hardware overdub-latency
calibration. Partial per-writer disk failures and long multitrack sessions are
not covered by this healthy-path test. Physical overdub playback is a separate
user check on the working AudioBox ASIO build. That check subsequently passed by
user report: both backing and new take play cleanly, with the original preserved.
Physical latency remains unmeasured.

## Validated manual saves and recovery/relink regressions

Manual project saves now validate the serialized session's roundtrip before
writing. They write a candidate beside the destination, check write/flush status,
then verify its size and exact UTF-8 bytes in 4096-byte readback blocks. Only a
complete matching candidate replaces the destination through JUCE's existing
temporary-file replacement mechanism. Staged streams are closed before replacement
for Windows compatibility. Invalid session values that would be rejected or
silently clamped on reopening cannot overwrite the prior valid project.

`IDWDAWSessionSaveTests` deterministically injects no-output/partial write failures,
truncated output reported as successful, and same-size incorrect snapshots near
the beginning and beyond the first readback block. Failed attempts retain the
original project byte-for-byte and remove their temporary output. Exception
cleanup, invalid models, directory/blocked-parent destinations, Unicode/large
metadata, MIDI and missing-media references, valid replacement and repeated saves
are covered. Missing audio does not prevent saving its project references.
The internal writer seam is for fault injection; production always uses the checked
file writer. No disk permissions or volume capacity are changed for testing.

The recovery/relink regression executables were rebuilt against this save path.
They pass corrupt-copy fallback, failed-checkpoint retry, two-generation rotation,
document-owner isolation, preserved missing-media references, invalid/short audio
rejection, relink undo/redo, save/reopen/recovery and actual WAV playback/export.
The project persistence, record/edit roundtrip, extended recording and multitrack
regressions also pass locally. All fixtures are disposable; user projects and
source recordings are untouched. This is not a forced power-loss or full UI-crash
recovery qualification.

## Strict bounded project integers

Project/recovery parsing now requires integer-typed values and checks their full
64-bit bounds before narrowing. This covers the session version, meter numerator
and denominator, saved audio input assignment, synth preset, MIDI pitch/velocity,
and fade-curve selectors. Booleans, strings, decimal values, out-of-range values
and large wraparound aliases are rejected. Valid 32-bit/64-bit integer bounds and
legacy omitted optional fields remain supported. A failed parse leaves the live
session and recovery origin unchanged.

`IDWDAWProjectIntegerTests` reproduces the previous coercion defect before the
fix, then tests 104 malformed-field cases through in-memory, on-disk project and
recovery parsing. It also checks valid bounds in memory and serialized JSON and
the legacy optional-field defaults. The test uses disposable metadata fixtures;
it does not open microphones or change user data. The user-provided Session7 and
Session9 projects were additionally loaded read-only through the corrected parser
to verify compatibility.
