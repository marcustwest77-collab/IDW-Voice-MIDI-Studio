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

## Navigate the full piano roll

The piano roll now reaches all MIDI pitches 0–127 and later song sections.
Wheel vertically to change the pitch range; Shift+wheel or a horizontal wheel
scrolls time. Ctrl/Cmd+wheel selects one of five zoom levels (8–48 pixels per
beat), keeping the beat under the cursor fixed where the content bounds allow.
A header shows the navigation controls and a beat ruler. View state is separate
from notes, persistence and undo history. Navigation is held steady during a
drag; quarter-beat drawing, movement and resizing use the current zoom/scroll.

Shared viewport geometry clips notes before pixel conversion, so offscreen or
very large finite note positions are not narrowed to integers. A long note
clipped by the right edge is not mistakenly treated as a resize handle. Pitch
and time scroll limits adapt to the visible grid and track content. Project
load resets the view along with the editor's existing history reset.

PianoRollNavigationTests checks all 128 pitches at multiple viewport heights,
cursor-anchored zoom, scroll/zoom limits, clipped and nonfinite geometry, actual
component wheel/draw/resize handlers at pitches 0/127 beyond beat 149, gesture
navigation guards, undo isolation, persistence and the production painter.
The prior MIDI edit-history regression also passes with the new ruler offset.
Both local regressions compile under MSVC /W4 /WX. The new test is registered in
CTest and Windows/macOS foundation CI. Physical wheel/trackpad interaction
remains a separate user check; the installed beta has not been changed.

## Recall the project master mix

Projects now save master volume, low/high tone and output ceiling in an optional
`masterMix` block in the v1 project format. Opening or recovering a project
restores both its sliders and live audio parameters. Older projects without
the block load the original defaults (gain 1, tone 0 dB, ceiling -1 dB), instead
of carrying over the previously opened project's master settings. These older
files never contained master settings, so their earlier slider positions cannot
be reconstructed. Older application versions do not retain this new block when
resaving; use a supporting test build to preserve the recalled settings.

Master changes participate in recovery dirty detection and portable export.
WAV export now consumes the master settings from the captured session snapshot,
matching recalled playback parameters. The explicit-parameter audio renderer
remains available for reference mixes. Master ranges match the existing UI:
gain 0–1.5, tone -12–12 dB and ceiling -12–-0.1 dB. A present block must be
complete and numeric; malformed or out-of-range values are rejected before
replacing live state, saved projects or output WAVs.

MasterMixRecallTests checks actual disk save/reopen, identical mixed audio/MIDI
WAV samples before and after reopening, explicit master processing parity,
silent master gain, output ceiling, master-only recovery changes, portable
settings, legacy defaults, numeric boundaries, 46 invalid cases and unchanged
saved/source files. It passes under strict local MSVC /W4 /WX. Rebuilt save,
persistence, recovery, portable-project and strict-integer regressions also pass.
The new target is registered in CTest and Windows/macOS foundation CI. Full
application build and physical slider/playback recall remain pending validation.
Installed beta, working PR #211 application and user projects are unchanged.

## Standard MIDI arrangement export

OPEN / RECOVER > Export audible MIDI tracks writes a Standard MIDI File (.mid),
type 1 at 960 ticks per quarter note. A conductor track stores tempo (rounded to
the nearest microsecond per quarter) and meter; each audible MIDI track retains
its name, note pitches, velocities and timing rounded to the nearest tick.
Note-offs precede same-tick retriggers. Muted tracks and tracks excluded by solo
are omitted; an audio-only solo therefore leaves no MIDI notes to export.
Empty tracks and audio clips are omitted. This transfers notes, not audio,
built-in synth patches, master processing or external instrument assignments.
Choose instruments in the receiving DAW; a physical import into another DAW
has not yet been demonstrated.

Export supports up to 15 audible MIDI tracks on distinct melodic channels,
skipping percussion channel 10. It rejects additional tracks rather than
silently reusing channels. Invalid notes, notes rounding to zero ticks, durations
past four hours and meter denominators not expressible by SMF (not powers of
two) fail with a readable error. The project and existing MIDI files remain
unchanged. Export runs on the existing worker, reports progress, and observes
CANCEL EXPORT before publishing. A staged file is flushed, read back without
inventing missing note-offs, and checked event-for-event before publication.

StandardMidiExportTests passes strict local MSVC /W4 /WX compilation. It verifies
both JUCE readback and an independent binary decoder for SMF chunks, variable
length deltas, running status, metadata and the complete expected note event
sequence. Coverage includes fractional timing, velocities, Unicode track names,
tempo/meter, same-tick ordering, project save/reopen, mute/solo, all 15 separate
melodic channels, the four-hour tick boundary, malformed data, cancellation,
failed output creation, unchanged source session and existing-file protection.
The target is registered in CTest and Windows/macOS foundation CI. Fresh full
application builds and external-DAW import remain pending. No installed beta,
working PR #211 application or user audio/project was changed.

## Edit selected-note velocity

The piano roll now has a NOTE VELOCITY inspector below the grid. Select a note
and drag its slider or enter a value from 1–127. No selection leaves the control
disabled, and mouse-wheel changes on the slider are disabled to prevent an
accidental velocity edit while navigating. The inspector reserves its own footer
space; full-range pitch navigation uses the remaining grid height.

Each slider drag forms one MIDI undo entry, including its final value when a
slider notification is still queued. Typed changes are individual edits. No-op
changes retain redo. History, deletion and navigation cannot interfere with an
active drag. The existing busy guard prevents edits during playback, recording,
calibration capture or export. A stale selection fails safely if notes or track
identity changed; project reset clears the inspector, selection and gesture.
Undo changes only notes and preserves newer audio, master, mixer and tempo work.

MidiVelocityTests exercises production slider callbacks and model validation,
grouped undo/redo, final pending values, no-ops, busy/conflict/reset guards,
unchanged unrelated work, disk save/reopen, standard MIDI note velocity and
exported WAV amplitude at velocities 1 and 127. Strict MSVC /W4 /WX compilation
and this regression pass, along with rebuilt MIDI-history and piano-navigation
regressions. A software-rendered component preview was visually inspected for
footer layout; this is not a live application screenshot or physical interaction
test. The target is registered in CTest and Windows/macOS foundation CI.
Fresh application builds and physical inspector/shortcut testing remain pending.
Installed beta, working PR #211 app and user audio/projects remain unchanged.

## Repeat a selected MIDI note

The piano roll now offers DUPLICATE NOTE and Ctrl/Cmd+D. It appends a copy
immediately after the selected note with the same pitch, length and velocity,
then selects the copy so repeated duplication builds a sequence. Each repeat is
one undo step. The view follows a copy extending beyond the right edge, without
altering the musical timing. The button shares the velocity footer and hides
in narrow editors; the keyboard operation remains available with editor focus.

The operation refuses busy/dragging states, stale selections, wrong/missing
tracks, invalid note metadata, floating-point timing overflow or collapsed
duration, and the 100,000-note track limit. Failed operations do not add history
or change the source note list. Undo preserves unrelated audio, tempo, mixer and
master work. The copied notes use the existing persistence, synth and MIDI-file
export paths.

MidiDuplicateTests exercises the production button/selection operation, repeat
selection, undo/redo, busy/stale/drag guards, capacity/overflow checks, narrow
layout, new-note visibility via the production painter, project save/reopen,
actual exported MIDI timing/velocity and every repeated note's WAV samples.
Strict MSVC /W4 /WX compilation and this regression pass. Rebuilt velocity,
MIDI-history and piano-navigation regressions are also checked. A software-rendered
preview was inspected for footer layout. The new test is registered in CTest
and Windows/macOS foundation CI. Full application builds and physical duplicate
button/shortcut testing remain pending. The open PR #223 test app, installed
beta and user projects/audio have not been replaced or modified; the pending
AudioBox listening check is still unconfirmed.

## Nudge selected MIDI notes with the keyboard

With the piano roll itself focused, Left/Right moves the selected note by a
quarter beat and Up/Down transposes one semitone. Shift changes the steps to a
whole beat or octave. Ctrl/Cmd/Alt+arrow combinations are left to other controls;
text fields keep their own keys. Each successful move is one MIDI undo step,
retains selection, and preserves note length and velocity. Time moves stop at
beat zero; pitch moves requiring values outside 0–127 are refused as a whole,
so an octave command never silently applies a partial octave.

The view follows the selected note in both time and pitch. The same reveal
helper now serves note duplication. Busy/dragging states and stale selections
cannot mutate notes. Invalid targets, nonfinite timing, overflow, collapsed
duration and pitch-integer overflow are refused. No-op/boundary attempts do not
clear redo. Undo preserves unrelated audio, tempo, mixer and master work.

MidiNudgeTests checks the exact shared arrow decoder and production selected-note
operation, all simple/Shift mappings, modifier exclusions, undo/redo, no-op redo
preservation, unfocused/busy/note-drag/velocity-drag/conflict guards, timing and
pitch bounds, and note visibility through the production painter. Disk
save/reopen and actual exported MIDI events retain the edited note. The rendered
WAV is checked for its shifted onset and against an independent C-sharp-5 sine
oracle. Strict local MSVC /W4 /WX compilation and this regression pass.
Neighboring duplication, velocity, history and navigation regressions are
rebuilt as well. The new target is registered in CTest and Windows/macOS
foundation CI. Full application builds and physical shortcut delivery remain
pending; installed beta, the separate #223 test app and user projects/audio are
unchanged. The AudioBox listening check remains unconfirmed.

## Halve or double MIDI note duration

Select a piano-roll note and use HALF LENGTH / 2x LENGTH, or the unmodified
`[` / `]` keys with the editor focused. Each successful operation changes only
duration, retains selection, and creates one undo step. Start, pitch and velocity
remain unchanged. The view follows the resulting note endpoint. The buttons
share the footer with velocity and duplication and hide below 760 pixels; the
keyboard operation remains available. Text fields keep their own keys.

The checked model operation refuses invalid targets, malformed note metadata,
nonpositive/nonfinite factors, multiplication overflow/underflow, unrepresentable
endpoints and no-ops. Busy states, note/velocity drags and stale selections are
guarded. Failed/no-op operations preserve redo and notes. Undo preserves newer
audio, tempo, mixer and master work. Very short positive durations still face
the existing standard-MIDI export rule rejecting notes that round to zero ticks.

MidiLengthTests exercises production buttons, repeat selection, undo/redo,
no-op redo preservation, busy/unfocused/drag/stale/reset guards, numeric limits,
footer layout and actual project save/reopen. It checks the exported MIDI note
pair and verifies the shifted WAV endpoint/release while comparing unchanged
pitch/velocity samples in the shared duration. The software-rendered footer
preview was visually inspected. Strict local MSVC /W4 /WX builds and all six
regressions pass: length, nudge, duplication, velocity, MIDI history and piano
navigation. The new target is registered in CTest and Windows/macOS foundation
CI. Full app builds and physical button/shortcut delivery remain pending. The
installed beta, separate #223 test app and user projects/audio are unchanged;
the AudioBox listening check is still unconfirmed.

## Remember the chosen audio interface

The Workstation now loads JUCE audio-device state before opening audio, and saves
explicitly chosen state after idle device changes and before shutdown. Preferences
live in the per-user application-data folder at
`IDW Neo Studio/AudioDeviceSettings.xml`, separately from projects, recovery and
audio files. They include driver type, input/output names, sample rate, buffer
size and active-channel masks. This is intended to retain an AudioBox ASIO setup
between launches instead of asking the user to select it each time.

JUCE's saved-state/default-on-failure behavior is used through
`setAudioChannels(8, 2, savedState)`. If the preferred interface cannot open,
JUCE may fall back to an available default; the UI still shows the actual device
and recording remains disarmed. A null state (no explicit choice) does not erase
the saved preference. The store writes JUCE's last chosen state rather than
reconstructing preferences from an incidental fallback. The relevant contract
and XML format were checked against the pinned JUCE 9.0.2 manager source.

Preference reads are bounded to 64 KiB and reject malformed roots, invalid
numbers/masks, excessive XML structure and DTD/entity declarations before parsing.
Missing or bad preferences use the existing default startup path. Saves validate,
stage, flush and verify exact bytes and parsed XML before replacing the previous
file; unchanged state avoids a rewrite. Failure leaves current audio active and
reports a local status/log message. No file I/O is added to audio callbacks.

AudioDevicePreferencesTests checks exact ASIO-style state recall (including names,
rate, buffer, masks and MIDI metadata), a fresh store instance, null/default
preservation, unchanged-state writes, failed/partial/same-size-wrong writes and
exceptions, bad XML/numeric/mask data, entity/depth/size limits, disabled inputs,
and blocked destinations. Strict local MSVC /W4 /WX compilation and this test pass;
the rebuilt callback-isolation regression also passes. Tests never enumerate or
open drivers. The new target is registered in CTest and Windows/macOS foundation
CI. Full application builds, actual AudioBox restart recall and hardware fallback
remain pending physical validation. Installed beta, separate #223 test app and
user projects/audio are unchanged.

## Device recall through the real JUCE manager

AudioDeviceRecallIntegrationTests now exercises the actual JUCE
AudioDeviceManager lifecycle with two injected simulated audio backends. The
test overrides the audio-driver factory entirely; it does not enumerate or open
real audio drivers. The simulated ASIO label is test data, not a physical ASIO
qualification. The CI target shares the application's audio-backend flags.

Coverage starts with default startup (no chosen state), then makes an explicit
choice through JUCE and saves JUCE's generated XML. A fresh manager restores the
interface, 48 kHz rate, 128-frame buffer and second-input/stereo-output masks.
A registered callback receives those settings during real manager preparation.
The test verifies close/restart preservation, missing-interface startup using a
simulated fallback without replacing the saved choice, reconnect restoring the
original choice, and an explicit user change replacing that choice on disk.
Additional cases cover no available devices, simulated device-open failure and
bad-file startup, all preserving the appropriate saved bytes.

An interface that remains listed but refuses to open is distinct from one that
is absent. The former test verifies preference preservation; it does not promise
that JUCE always selects a different working backend after every open failure.
The user may still need AUDIO DEVICES when a driver cannot open.

The integration regression passes strict local MSVC /W4 /WX compilation and is
registered in CTest and Windows/macOS foundation CI with a 30-second timeout.
Full application builds and physical AudioBox unplug/restart/reconnect behavior
remain pending. No installed beta, separate #223 test application, active device
settings or user projects/audio were changed by this test-only step. The pending
AudioBox listening check remains unconfirmed.

## Show when the saved interface is not active

The recording-readiness strip now shows amber AUDIO FALLBACK with the actual
output name if the current driver or explicitly saved input/output identity
differs from the remembered choice. Its tooltip, the recording button and the
input selector include saved-versus-active setup details and direct the user to
AUDIO DEVICES. NO AUDIO DEVICE appears when no device is open, including a
device object that still exists but reports itself closed. Empty projects also
show the status without requiring a selected track.

Matched/default setups retain the normal readiness display; reconnection or an
explicit new selection clears the fallback message. The red RECORDING indicator
retains priority during capture. Status computation runs at startup and on
device-change notifications, not in the audio callback. It does not select a
device, arm tracks, alter volume, block recording or rewrite preferences.
This diagnosis compares interface identities; it is not a physical audibility
check or a promise that every saved rate/buffer value was accepted by a driver.

AudioDeviceRecallStatusTests checks backend/input/output mismatches, unavailable
and default cases, explicit replacement, JUCE legacy-name precedence, Unicode,
bounded single-line display and unchanged supplied XML. The existing real-JUCE
simulated-device integration test now verifies status across restoration,
fallback, reconnection, explicit choice, no devices, open failure and a closed
device object. Both regressions pass strict local MSVC /W4 /WX compilation. No
real audio drivers are opened. The status test is registered in CTest and the
Windows/macOS foundation workflow. Full app builds, the actual readiness-strip
interaction and physical AudioBox behavior remain pending. Installed beta,
separate #223 test app, active settings and user projects/audio are unchanged.

## Preflight verifies an open device and an actual folder write

PREFLIGHT previously accepted a device pointer with a cached positive sample
rate even if the device was closed, and a directory's existence without testing
write access. The shared recording check now requires an open device and a
finite positive live rate matching the prepared engine. Readiness, preflight
and single/multitrack start guards use that check. Start is rejected before
automatic arming or writer creation when the device/rate check fails; the stop
and existing take-finalization path remains available after device failure.

An explicit PREFLIGHT run now writes and flushes a tiny uniquely named temporary
marker in the recording folder, verifies exact readback, and cleans it up. It
never uses a take filename or overwrites existing media. A failed write,
readback or cleanup prevents PASS. The existing 100 MiB free-space threshold
still applies to preflight, without guaranteeing sustained recording capacity.
The dialog now uses real line breaks, shows open/rate agreement and the actual
write-probe result, and explains that neither mic audio nor audible playback
has been tested. Record-start writers retain their own error checks; the folder
probe runs only during explicit PREFLIGHT, outside audio callbacks.

RecordingPreflightTests checks closed/missing/stale/nonfinite device cases,
every required preflight flag, free-space bounds, new and existing folders,
successful write/flush/readback/cleanup, failed/partial/same-size-wrong writes,
exceptions and blocked destinations, with existing take bytes unchanged. The
real-JUCE simulated-device integration test also verifies live/prepared rate
agreement and a closed device object against the same production helper.
Both regressions pass strict local MSVC /W4 /WX compilation; no real audio
drivers are opened. The target is registered in CTest and Windows/macOS
foundation CI. Full app builds, physical PREFLIGHT and AudioBox recording remain
pending. Installed beta, separate #223 app and user projects/audio are unchanged.

## Preflight covers every armed audio destination

PREFLIGHT previously evaluated only the selected track, while RECORD can capture
all armed audio tracks. An occupied, duplicate-routed or disabled second armed
track could therefore be missed. It now inspects the complete explicitly armed
audio-track set, independent of the selected editor/mixer track, and requires
every destination to be empty with a distinct enabled input within the recorder's
simultaneous-track capacity. Device, rate, output, folder-probe and disk-space
checks remain shared requirements.

The report lists each armed destination and identifies occupied tracks, disabled
or out-of-range inputs and every affected duplicate route. Large arm sets have
bounded detail rows and show an omitted-row count. Inspection is read-only: no
tracks are armed, rerouted or cleared. RECORD's existing unambiguous auto-arm and
sole-active-input recovery behavior is unchanged; PREFLIGHT itself still expects
explicit arm/input configuration and reports ACTION NEEDED when none is armed.

ArmedRecordingPreflightTests checks independent selection scope, second-track
failures, duplicate routes, disabled/out-of-range channels, sparse physical-to-
callback mapping against the recording path, empty/MIDI/stale arm flags, eight-
track capacity, bounded/sanitized reports, unchanged session/arm state and the
shared device/rate/folder/space requirements. Strict local MSVC /W4 /WX builds
and both this regression and the prior recording-preflight regression pass.
No real drivers are opened; eight-route validation is simulated configuration,
not physical eight-input qualification. The new test is registered in CTest and
Windows/macOS foundation CI. Full application builds and physical multitrack
preflight remain pending. Installed beta, separate #223 app, active settings and
user projects/audio are unchanged; AudioBox listening is still unconfirmed.

### Piano-roll exit after track/project changes

User validation of the #231 test app exposed a visible but empty piano roll
with Audio 1 selected. STOP followed by PIANO ROLL did not close it. Inspection
confirmed that the toolbar validated the selected track as MIDI before deciding
whether to close an already-visible editor. Project restore reset the note
selection/history but left the overlay visible. The user regained the main view
by selecting MIDI 1 and toggling PIANO ROLL; no take needed to be cleared.

The editor now owns its tested toggle operation: closing works even when the
current selection is audio, out of range or unavailable; opening still requires
a valid MIDI track. The toolbar retains its existing playback/record/export
guards. A BACK TO TRACKS button provides an explicit exit inside the editor.
Project reset hides the overlay while discarding the old selection/gesture/
history state. Ordinary close retains note edits and finalizes pending velocity
gestures using the existing grouped undo behavior.

PianoRollCloseTests exercises the production component toggle and back-button
callback, audio/invalid/missing-session close paths, refusal to open invalid
destinations, unchanged projects for view-only operations, pending final velocity
and undo/redo, header bounds and project replacement/reset. It and rebuilt
MidiVelocityTests and MidiLengthTests pass locally under MSVC /W4 /WX. The new
regression is registered in CTest and Windows/macOS foundation CI. Full app CI
and physical verification of the fixed build remain pending. The installed beta,
running #231 test executable and original user projects/audio are not replaced.

Separately, user-reported #231 AudioBox ASIO validation passed initial preflight,
short recording/waveform/clean playback, occupied and duplicate input diagnostics,
save/reopen after restarting the app, and WAV export playback in an external
Windows player. The generated MIDI fixture played both notes with the second
louder, and the user confirmed velocity 110 -> 40 -> undo 110 -> redo 40.
MIDI edited-project save/reopen is still pending; no physical eight-input,
calibrated-latency or live two-computer qualification is inferred.

### Explicit MIDI history controls and numeric-entry regression

Further user validation confirmed that double-clicking NOTE VELOCITY's number,
selecting its text, entering 40 and pressing Enter commits the edit. The live
Ctrl+Z/Ctrl+Y retry did not restore 110 even after note reselection. The earlier
reported shortcut pass is therefore under recheck. Accessibility observations
continued to report the voice-command field as focused; this is evidence to
investigate, not proof of the root cause. The current live shortcut failure is
not claimed fixed or physically qualified.

Piano-roll UNDO MIDI and REDO MIDI header buttons now invoke note history directly,
without requiring the grid to receive a keyboard shortcut. They reflect undo/
redo availability, refuse changes during a gesture or when editing is blocked,
commit an active numeric editor before restoring history and preserve existing
stale-history conflict checks. Restoring history still clears note selection;
tooltips tell the user to reselect a note to inspect its restored value. Narrow
editors hide the history buttons to keep BACK TO TRACKS accessible.

MidiVelocityTests now exercises JUCE's real slider number editor using
showTextBox, the child TextEditor and hideTextBox(false), rather than only
hand-invoked drag callbacks. The unchanged history model passes the numeric
110 -> 40 -> undo 110 -> redo 40 path locally. New checks exercise the actual
history button callbacks, availability, a still-pending numeric edit, busy/drag
guards, conflicts, reset and header bounds. Rebuilt MidiVelocityTests,
PianoRollCloseTests and MidiLengthTests pass with MSVC /W4 /WX. A rendered 1000px
editor preview was inspected for button/footer overlap. Existing CI targets run
the expanded regression; full app CI and physical verification remain pending.
Parent #232 Windows CI was still running and two macOS jobs were cancelled at
the time of this change, so those checks are not reported as passed. This source
change does not replace the installed beta or the running #231 test app.

### Recover history controls after a released gesture

The #233 Windows artifact passed all five CI jobs and 68 DAW regressions on each
application platform, and was downloaded, checksum-verified and launched as a
separate test copy. The user saw the new controls, but reported that UNDO MIDI
did not restore 110. Accessibility inspection showed velocity 40 enabled while
UNDO MIDI, REDO MIDI, duration and duplicate controls were disabled. This matches
an unfinished gesture's control state. The exact native event sequence producing
that live state remains unconfirmed; the physical history-button check is failed,
not qualified by the green unit tests.

The regression now commits numeric entry using the actual TextEditor Return-key
path and dispatches its queued GUI event, in addition to the existing pending
entry test. This path passes locally. A separate regression recreates the
observed disabled state by withholding mouseUp after selecting an edited note.
History remains unavailable until that pending gesture is finalized.

The piano roll now checks physical mouse-button state at 20 Hz while visible,
on the message thread. Once no button is held, an unfinished gesture is finalized
using its existing final-value, history-grouping and conflict checks. A held
button still blocks history; hiding the editor stops the timer. This recovers a
missing release without discarding an existing edit or requiring an editor
close/reopen. It does not assert that the native release failure was reproduced.

Expanded MidiVelocityTests verifies the held/released distinction, unchanged
notes during a no-op recovery, velocity undo/redo after recovery, a lost slider
release with a pending final value, one-step grouping, repeated idle recovery and
conflicting newer notes. It, PianoRollCloseTests and MidiLengthTests pass locally
under MSVC /W4 /WX. The velocity CTest now has a 30-second timeout around the GUI
dispatch test. Full application CI and physical qualification of this recovery
are pending. Installed beta and the running #233 test app are not replaced.

### Invalid numeric entry must not destroy MIDI redo

Read-only inspection of user-created diagnostic saves established a controlled
40 -> 70 edit, then Undo back to 40 using the panel close/reopen workaround.
The subsequent reported Redo produced a saved velocity of 1, not 70. All copies
retain the same two C4 notes, positions and lengths. The current live Redo check
is failed; the earlier history-availability recovery is not a general history
qualification.

A new regression reproduces an exact velocity-1 failure: after Undo, open the
selected note's numeric editor, leave its text blank and invoke REDO MIDI.
Committing that field used JUCE's numeric coercion/clamp, creating a new edit to
1 and clearing the redo entry before it could be applied. The regression failed
against the previous implementation with actual velocity=1. This is a proven
input path matching the saved result, not proof that an empty field caused the
user's native interaction.

Velocity text parsing now accepts a trimmed decimal integer of at most three
digits in the range 1..127. Blank, malformed, fractional, out-of-range or oversized
input retains the current value without adding history or invalidating redo.
The expanded regression passes invalid inputs through the real numeric editor
and redo-button callback, and verifies valid boundary, padded and trimmed values
with undo/redo. MidiVelocityTests, PianoRollCloseTests and MidiLengthTests pass
locally under MSVC /W4 /WX.

Parent #234 macOS CI failed its windowless native Return-key dispatch test at
"numeric entry did not commit". Windows keeps that queued key-event test; other
platforms now exercise JUCE Label's actual protected Return-listener callback
through a test-owned Label subclass, without a native application loop. Both
test paths pass locally on Windows; macOS CI and native macOS key delivery are
not claimed verified. This changes the unit-test boundary, not production key
routing. Full CI and physical validation of the input fix are pending. The
installed beta, running #233 test executable and all diagnostic copies are left
intact.

### Notes-only standard MIDI import

OPEN / RECOVER now offers Import MIDI notes. The file is decoded and staged
before an Add tracks / Cancel preview. The preview reports track/note counts,
file tempo/meter, current project tempo/meter and the initial Keys instrument.
Imports add separate MIDI tracks per source track/channel and follow the current
project's musical tempo/meter; existing timing, audio references, instruments,
mix settings and notes are preserved. An active project solo is reported in the
preview. A changed project or busy transport prevents the staged commit.

The strict SMF reader accepts formats 0/1 with PPQ timing, running status,
velocity-zero note-offs and FIFO pairing of overlapping same-pitch notes. It
requires complete chunks, matched positive-duration notes and end-of-track
markers. Limits are 1 MiB, 100000 events, 50000 notes, 128 imported destination
tracks and four hours at both file and destination tempos. Tempo and meter must
be constant. SMPTE timing/offsets, SysEx, controllers, program changes and
expression events are rejected with no partial import. This initial import is
for note data, with track names bounded/sanitized and new instruments set to Keys;
it does not reproduce an external synth's patches, controllers or release-velocity
response. Note-off velocities are validated but not represented in the model.

The model plan owns a private staged session. Merely preparing or cancelling a
preview cannot change the destination; commit checks its complete current JSON
against the captured state before replacing the model. A second commit or a
changed project fails without overwriting newer work. Audio player indices are
preserved because only new tracks are appended, and MIDI history is retained.

StandardMidiImportTests passes under local MSVC /W4 /WX. It covers native export/
import roundtrip, metadata, channel splitting, running status, FIFO overlaps,
all truncated prefixes, invalid chunks/status/notes/tempo/meter, file/event/note/
track limits, 1500 deterministic mutations, stale/double commits, unchanged
existing project JSON and imported-note save/reopen. It also imported the user's
already-exported two-note .mid read-only with tempo 120 and velocities 30/70;
that file's bytes are unchanged. The test opens no audio drivers. The new target
is registered in CTest and Windows/macOS foundation CI. Full application CI and
physical import UI/playback qualification are pending; no external-DAW import
demonstration is inferred. Installed beta and the #235 test app are not replaced.

### New empty project (2026-11-03)

OPEN / RECOVER > New empty project now offers a confirmation before switching
to empty Audio 1 and MIDI 2 tracks at 120 BPM, 4/4 with default mix settings.
Cancel (also Escape or Return) keeps the document. Unsaved changes must pass
the existing validated recovery checkpoint before replacement; failure keeps
the current work open. The prompt advises SAVE PROJECT for a named copy and
points to Recover unsaved work. Saved projects and source audio are not deleted
or overwritten. Clean documents need no redundant recovery write.

Confirmation captures the complete session JSON and project path. A changed
document, busy transport/recording/export/edit or reused confirmation is rejected.
The new document has a fresh recovery owner so saving it cannot remove the
previous document's recovery copy. The workstation stages its empty audio slot,
then clears old sources, waveforms and audio/MIDI edit histories, resets the
playhead and scroll, restores default mix, disarms inputs and turns monitoring off.
Audio device preferences remain available for the next recording.

NewProjectTests passes locally with MSVC /W4 /WX. It exercises Cancel, busy and
stale confirmation, changed project path, checkpoint failure/retry, named and
untitled work recovery, retained notes/mix/media paths, one-use commit, fresh
defaults, and byte-preserved original project/audio. It also verifies that saving
the new document leaves the prior recovery copy available. CTest and Windows/macOS
foundation build registration added. Full application CI and physical new-project
UI/recovery checks are pending; the installed beta and running #236 test app are
unchanged. No external DAW or live two-computer qualification is inferred.

### Project name and save status (2026-11-03)

The header now shows the current project filename and Saved, Unsaved changes,
Not saved yet, or Recovered - save required. The native window title repeats
this information, including while another editor covers the main view. Hover
over the header status for the complete path; untitled projects explain that
SAVE PROJECT chooses a filename. Long names can be clipped in the header;
the tooltip retains the full path and status.

A message-thread timer refreshes status twice a second independently of transport
and recovery polling. Dirty detection directly compares persisted session fields
against the last successful save/open baseline without JSON serialization or
disk access. Reverting to the saved state clears the dirty indicator. Recovery
opens remain marked save-required until a successful manual save. Recovery
autosave never updates the saved baseline. New empty projects reset to Untitled
project / Not saved yet; successful save-as changes the displayed filename.
Failed/cancelled opens, saves and new-project requests do not establish a baseline.

ProjectDocumentStatusTests passed under local MSVC /W4 /WX. Tests cover fresh,
edited, saved, reverted, reopened, recovered, saved-as and new-project states;
real save/load and recovery checkpoint/read; full path display; all 35 persisted
field/collection edits; non-persisted track properties; and a change to the last
of 50000 notes. CTest and foundation CI targets registered. Application CI and
physical header/window-title checks are pending. Installed beta and user project
and audio files are unchanged. The parent #237 physical workflow remains pending.

### Project save destination (2026-11-04)

SAVE PROJECT suggests the current project's folder and filename, or IDW Session
for an untitled document. A dedicated normalizer collapses repeated .idwproj
suffixes and separator dots: Song.v2.idwproj..idwproj becomes Song.v2.idwproj.
It preserves dotted stems and Unicode names, appends the project extension to
other typed names, and rejects an empty stem. Existing oddly named files are
not renamed or deleted.

Replacement is confirmed against the final normalized full path, independently
of the native picker. Cancel/Return/Escape keeps the target. No existing final
destination is written without explicit replacement consent, including a file
that appears after selection. A model snapshot and originating project path
guard against changes during the picker or replacement prompt; busy or reused
save requests are rejected. Write failure leaves the current filename/status
and recovery state unchanged; only a successful validated save updates them.

ProjectSaveDestinationTests passed under local MSVC /W4 /WX: normalization and
idempotence, Unicode/dotted stems, current-project suggestion, final-path
replacement consent, stale/busy/double requests, a newly appeared target,
directory rejection, blocked-parent failure/retry, saved MIDI/tempo data and
unchanged source audio. CTest and Windows/macOS foundation targets added. Full
application CI and physical save-dialog checks remain pending. User projects,
the running #238 test app and installed beta remain unchanged.

### Rename selected track (2026-11-04)

OPEN / RECOVER > Rename selected track opens a prefilled single-line name
editor for the selected track. Rename applies a trimmed 1–64 character name;
Cancel leaves it unchanged. Unicode and duplicate display names are supported;
control characters and line separators are rejected. A complete session/path
snapshot rejects stale, busy, invalid-track and reused dialogs. Pending note
gestures are finalized before the dialog is prepared. Successful renames refresh
the track/mixer display and project dirty status without changing audio paths,
notes, clips, input routing, instruments or mix.

MIDI history entries use track names as an identity guard. Explicit rename
retargets matching entries in both undo and redo stacks, preserving note edits
while keeping the guard against unrelated track replacement. Clip undo/redo
already restores clips only and retains the renamed metadata. Renaming itself
is a metadata edit and is not a MIDI-note or clip-history entry.

TrackRenameTests passed under local MSVC /W4 /WX, covering Unicode/trim/bounds,
invalid names, no-op/duplicate names, stale/busy/path/track/double-submit guards,
save/reopen, exact preservation of other session JSON, dirty status, multiple
note undo/redo entries across tracks, retained identity conflicts, audio clip
undo/redo and unchanged original project/audio fixtures. CTest and foundation
CI targets registered. Application CI and physical rename UI/playback checks
are pending. Parent #239 Windows physical save tests passed, while its macOS
application job remains cancelled/unqualified. Installed beta and user files
are unchanged; no live two-computer collaboration claim is made.
