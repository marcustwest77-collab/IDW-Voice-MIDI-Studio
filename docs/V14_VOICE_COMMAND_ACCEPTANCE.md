# V14 Voice Command Acceptance — Windows 10 / AudioBox USB 96

Status: **source inspected; physical-device tests NOT RUN for current beta**. The `DAW/Main.cpp` on `v14-daw-foundation` already contains Windows SAPI one-shot speech recognition via TALK, command input, history, arm/disarm, transport, record confirmation, track selection, mixer commands, and project save. Do not claim that recognition works on the tester's machine until tested.

## Test setup
- Keep the known-working portable V14 directory intact. Use a separate copy of a newly built executable and a disposable project.
- Log build commit, `BUILD-INFO.json`, Windows version, AudioBox USB 96 driver, selected input channel and output, sample rate and buffer.
- Use headphones, avoid speaker feedback. Start with no sensitive session or important recording armed.
- Test all commands **first by typing** in the console, then by pressing TALK and speaking clearly. Confirm that any recognized transcript is visible before accepting the outcome.

## Voice QA checklist

| ID | Input / scenario | Expected result |
|---|---|---|
| V01 | TALK then silence | Stops listening after approximately eight seconds; no DAW transport action |
| V02 | TALK then cancel | Discards buffered recognition; no command runs |
| V03 | `IDW play`, `IDW stop` | Transport starts/stops if permitted |
| V04 | `IDW select track 3` | Selects track 3 only if present; out-of-range track rejected |
| V05 | `IDW arm track` on audio track | Arms only selected audio track; feedback shows state |
| V06 | `IDW arm track` on MIDI track | Refuses to arm audio capture |
| V07 | `IDW record vocals` | Recording does **not** start; asks for confirmation |
| V08 | `IDW confirm recording` promptly after V07 | Recording starts only with valid input/armed track and idle transport |
| V09 | Confirm without request, or >15 s after request | Does not start recording |
| V10 | Request then `IDW cancel record` | Clears pending request; later confirm rejected |
| V11 | `IDW stop recording` while recording | Stops capture without corrupting WAV |
| V12 | `IDW add audio track`; `IDW next track` | Adds/selects requested track only when idle |
| V13 | `IDW overdub on`, then `off` | Toggles beat accompaniment while idle; no unintended bleed |
| V14 | `IDW mute`, `unmute`, `solo`, `unsolo` | Changes the selected track, not another track |
| V15 | `IDW save project` | Opens save UI; no silent overwrite |
| V16 | Unknown or misrecognized phrase | No transport, recording, track or destructive side effect |
| V17 | Commands during recording/export | Prohibited actions rejected while busy |
| V18 | Multiple recognitions; closing DAW during listening | No crash, stale callback or unexpected delayed command |
| V19 | SAPI unavailable / mic disconnected | Helpful error; typed command console remains usable |
| V20 | Review command history | Recent commands and results visible; no full audio recordings persisted in command history |

## Observed source risks and follow-up engineering
1. A recognized one-shot SAPI transcript is passed directly to `executeStudioCommand` on the message thread; **confirmation does not require a distinct spoken command source**, so test spurious speech, background lyrics, and recognition mistakes before enabling in actual sessions.
2. Source comments and UI tooltip disagree about whether Windows native speech recognition is integrated. Update outdated tooltip/help text in a follow-up code change.
3. The voice command `arm track` operates on the **currently selected** audio track; `arm track three` is not supported by that branch unless separately implemented. Do not advertise numbered arming without code and tests.
4. Verify selected input is the AudioBox mic and output is headphones. Keep speaker playback out of the speech-recognition microphone to avoid inadvertent commands.
5. No always-listening wake-word feature or external AI voice provider is required by the current one-shot implementation.
6. Before extending to remote collaborator commands, add explicit per-user authorization, recording consent, revocation, audit events and reliable disconnect behavior.

## Reporting
For each case record PASS/FAIL/BLOCKED, whether input was typed or spoken, transcript recognized, actual result, build SHA, and issue reproduction. **A successful compile is not a substitute for microphone tests.**
