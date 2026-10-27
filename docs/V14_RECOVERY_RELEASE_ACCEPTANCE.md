# V14 Windows Beta — Recovery and Release Acceptance (2026-10-26)

This record distinguishes **recovered user test reports**, **CI results**, and **unverified release gates**. It must not be interpreted as certification of a new executable. Do not overwrite an earlier working beta or use the only copy of an important project.

## Exact build baseline

- Source branch: `release/v14-windows-packaging`
- Inspected branch head: `c9fa62d03aa472e25eead3985397e232eb81bfba`.
- GitHub Actions: [V14 DAW Foundation Build #583](https://github.com/marcustwest77-collab/IDW-Voice-MIDI-Studio/actions/runs/39319950285). The Windows and macOS build/test jobs were reported successful; Windows packaging and artifact upload steps succeeded.
- The archived Windows beta and installer from this run are not permanent release downloads. Their artifact retention/expiration is controlled by GitHub Actions.
- For any **new** build record `BUILD-INFO.json` source commit, workflow run, `file_version`, `signature_status` and the per-file SHA256 values. A filename or version label alone cannot prove build identity.
- Automated compilation and tests cannot substitute for Windows 10 Home + PreSonus AudioBox USB 96 physical-device QA.

## Recovered *older-beta* testing — NOT transferable to a new build

Earlier user reports indicated:
- AudioBox USB 96 microphone on input 1 worked after resolving Windows routing problems.
- Beat + recorded voice could play together; a 60-second sync check was reported passed.
- Separate overdubs through tracks 3–6 worked, and save/reopen preserved tracks.
- Exported WAV played back with beat and vocals.
- Ten-minute recording stayed clean and synchronized.
- A relocated portable beta launched, recorded, saved and exported.

These are valuable historical reports, **not fresh measurements**. Do not assume new binary has identical behavior.

## Mandatory Windows QA — record pass/fail and evidence per build

Use a duplicate, disposable project; retain earlier beta in its original folder.

| Gate | Test | Pass evidence | Result |
|---|---|---|---|
| W01 | Verify ZIP/installer SHA256 and `BUILD-INFO.json` against chosen run | Hashes, commit, signature status | NOT RUN |
| W02 | Run portable beta from a new extracted folder | Launch/no missing dependency/crash | NOT RUN |
| W03 | AudioBox USB 96 input 1 and headphone output selection | Driver, sample rate, buffer size, meter/playback | NOT RUN |
| W04 | Arm fresh mono vocal track and capture 30 s | WAV exists, contains voice, audible replay | NOT RUN |
| W05 | Load beat, record vocals on separate track, listen to both | Independent tracks, no unintended beat bleed | NOT RUN |
| W06 | Record 3 or more overdubs on separate tracks | Tracks distinct and editable | NOT RUN |
| W07 | Verify beat/vocal start and end sync at 60 s | Audio comparison and timing notes | NOT RUN |
| W08 | Save/reopen project from a copied test folder | All media links, clips, settings preserved | NOT RUN |
| W09 | Export stereo WAV then listen externally | Correct beat/vocals, duration and no gaps | NOT RUN |
| W10 | 30-minute stress recording | No crash, dropout, silent/missing WAV or drift | NOT RUN |
| W11 | Copy portable folder to a second location on same PC | Launch, record, reopen and export | NOT RUN |
| W12 | Installer/uninstaller and rollback to working beta | No deletion of project, recordings or skins | NOT RUN |
| W13 | Confirm AI proposal safety while transport runs | Unknown commands rejected, changes require approval | NOT RUN |
| W14 | Confirm IDW Connect scope and basic video invite | Browser invite works; no false claims of DAW sync | NOT RUN |

Stop release if there are corrupt/missing recordings, persistent drift, unsafe feedback, project data loss or inability to roll back. Do not connect speaker output to a microphone input for loopback testing; use only supported line-level connections.

## Feature reality check

- Current IDW Connect UI launches a **Jitsi browser meeting**. It does not embed video, synchronize DAW sessions, send DAW audio, or enable remote control.
- MCP helper reads saved snapshots and proposes changes; **AI STUDIO** has manual review/approval. It is not an autonomous, live ChatGPT/Claude integration.
- Embedded video, two-person authentication, remote all-track control and voice-triggered recording remain future milestones requiring separate implementation and safety tests.
- Review any feature's **current code and tests**, not just the chronological development notes, before marking it finished.

## Next technical milestones (separate branches/PRs)

1. Finish/record W01–W09 baseline on current Windows beta before high-risk feature changes.
2. Implement voice commands with explicit arming, confirmation for destructive actions, cancel/stop and a local-only default; add unit tests and hardware test instructions.
3. Design authenticated two-person session transport and embedded video, with opt-in mic/camera; avoid confusing video-call audio with DAW record input.
4. Implement permission-scoped remote controls only after transport, authorization, disconnect/fail-safe, audit and conflict tests.
5. Repeat W01–W14 plus security/privacy validation before any release announcement.

## Test record template

- Test date/time:
- Tester / Windows build:
- Exact source commit and GitHub workflow run:
- Portable or installer:
- DAW file version and signing result:
- AudioBox USB 96 driver / input and output channels:
- Sample rate / buffer size:
- Gate IDs and PASS / FAIL / BLOCKED:
- Error text, evidence, and reproduction steps:
- Rollback result:
- Approval for release (if any):
