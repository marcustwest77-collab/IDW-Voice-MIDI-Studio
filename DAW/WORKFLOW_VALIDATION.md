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

## Portable project media reuse and relocation

Portable export copies each distinct normalized absolute source path once.
Split/repeated clips and references on different tracks share that copied asset;
different paths with the same basename receive distinct numbered filenames.
Each new copy is checked against its source bytes before the relative project
is saved. Unresolved relative inputs are rejected because this API has no project
base-directory context; the Workstation resolves paths when opening a project.
Existing destinations remain protected, stale errors clear on success, and
ordinary missing-source or metadata-save failures remove the newly created
partial package.

`IDWDAWPortableProjectTests` reproduces the previous repeated-copy behavior,
then verifies unique copies, same-basename separation, preserved clip/mix/MIDI
metadata, exact media bytes, and identical audio mixes after relocating the
package and making the original generated sources unavailable. It also checks
existing-destination protection, missing-media rollback, invalid metadata,
unresolved paths and default temporary cleanup. Its retained-evidence argument
requires a fresh directory. All destructive fixture operations are confined to
newly generated test data.

A separate portable copy of the user-provided Session9 was created locally with
the corrected exporter. Its two previously verified WAVs total 72,110,800 bytes;
the former per-clip copying would use 220,387,048 bytes. Its project uses relative
audio references and the original project bytes are unchanged. User audio was
copied locally only; no recordings were uploaded. This is a portable copy on the
same computer, not an independently stored disaster-recovery backup.

## Portable copies stay portable after saving

The Workstation save handler now stores references to media inside the chosen
project directory as relative paths, while retaining absolute references to
external media. It saves a cloned session, so live reader paths and the
in-memory project remain unchanged. Missing internal references are preserved;
unresolved live relative paths fail without overwriting the previous project.
Save As into a different directory preserves links to assets outside that
directory. Media files are never automatically moved by Save.

The portable-project regression first reproduced the old behavior: opening and
resaving a portable project wrote absolute references, defeating a later move.
It now saves, moves and reopens the package a second time and checks every stereo
mix sample. Additional checks cover unchanged live paths, mixed internal/external
references, missing internal media, unknown relative bases and Save As behavior.

The actual Session9 portable folder was additionally compared with the source
project: all metadata matches after path normalization, and every source WAV
SHA-256 matches its packaged copy. A local ZIP keeps the project and Audio folder
together. All five ZIP entries passed CRC verification, and each content entry
matched the SHA-256 manifest. The ZIP is 61,458,533 bytes with SHA-256
`2fe720230a0e65fbabcb80063b907362735e0e8a77605f728bb980d0db5f556a`.
No audio was uploaded; originals and installed beta remain unchanged. Physical
listening of the moved/resaved real package is still a separate user check.

## Built-in MIDI instruments in WAV export

WAV export now mixes the five built-in MIDI instruments with audio clips before
the existing master EQ, gain and ceiling. MIDI-only sessions export successfully,
and notes extending past the final audio clip retain their complete timeline.
Track gain, pan, mute and solo apply across both track types. Export duration
includes muted material, matching the existing audio timeline behavior. Invalid
note/envelope data is rejected before output creation. External MIDI devices and
VST instruments are outside this built-in synth export path.

The shared synth now normalizes simultaneous voices per sample rather than per
callback, fixing volume differences when note boundaries meet different device
buffer sizes. Rendering uses a fixed 1024-frame stack buffer without heap
allocation. MidiBounceTests compares every sample across 1, 17, 64, 127, 256,
1024 and 2048-frame calls, all five presets at 44.1/48 kHz, saved/reopened
settings, MIDI-only and mixed WAVs, longer MIDI tails, mute/solo, master gain,
invalid inputs, cancellation and overwrite refusal. Independent sine and
simultaneous-voice oracles check pitch, amplitude, envelope, velocity and pan.
The target is registered in CTest and the Windows/macOS foundation workflow.

Local strict compilation and this regression pass, as do the existing bounce,
clip transport parity and MIDI benchmark executables. The local 128-note
benchmark averaged 0.808 ms per 256-frame buffer (5.333 ms budget). One of 256
timed 512-frame blocks took 34.957 ms, exceeding its 10.667 ms budget; no hardware
stability claim follows from this benchmark. Full application builds await CI.
The installed beta and working PR #211 test application remain unchanged; this
feature has not yet received a physical listening check.

## Piano-roll undo and redo

The piano roll now supports Ctrl/Cmd+Z, Ctrl/Cmd+Shift+Z and Ctrl/Cmd+Y for
drawing, moving, resizing and deleting notes. Each completed drag forms one
undo step; selecting without changing notes preserves redo. Newly drawn notes
are selected immediately. Delete/Backspace requires editor focus and no keyboard
modifiers. Editing and history restoration are blocked during playback,
recording, calibration capture and export by the Workstation's edit guard.

History retains at most 64 edits and restores only the affected MIDI note list.
It preserves tempo, audio clips, mixer and instrument settings. Unexpected note
changes, removed tracks and renamed targets fail without overwriting newer work;
the Workstation reports a conflict. An external note change during a gesture
also stops that gesture without capturing the external edit into undo. Loading
a different project clears both MIDI history stacks and pending selection/drag
state. Hiding the editor or changing its track finishes the current gesture.

MidiEditHistoryTests drives the production component's draw/move/resize mouse
handlers and the deletion/history operations used by keyboard shortcuts, without
opening a window. It checks gesture grouping, no-op redo preservation, deletion,
busy/unfocused guards, conflict preservation (including an external edit during
a drag), project reset, the history cap, and saved/reopened edited notes.
Strict local MSVC /W4 /WX compilation and the regression pass. The target is
registered in CTest and Windows/macOS foundation CI. Interactive shortcut
delivery still needs a physical UI check; no installed application was replaced.
