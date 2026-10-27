# V14 numbered voice track arming — review and QA

Branch: `feature/v14-numbered-track-arming-20261026`. Source implementation: `DAW/Main.cpp`.

The command console and Windows TALK transcript now accept the following exact phrases:
- `IDW arm track 3`, `IDW arm track three`
- `IDW disarm track 3`, `IDW disarm track three`
- Words one through ten, or numeric digits for larger track counts.

Design rules:
- Resolve the explicitly supplied track. **Never silently arm the previously selected track** on malformed input.
- Only audio tracks can be armed; refuse a MIDI destination.
- Reject changes during transport playback, recording, export and loopback capture.
- If accepted, arm/disarm only the indicated audio track, select it, refresh mixer and show feedback.
- This patch does not alter the recording confirmation requirement.

## Tests to perform on a disposable project
1. Add three audio tracks. Enter `arm track three`; confirm only track 3 is armed.
2. Enter `disarm track 3`; confirm track 3 is disarmed without touching tracks 1–2.
3. Enter `arm track 999`; confirm no track changes and an error is shown.
4. Enter `arm track banana` and `arm track 0`; confirm no track changes.
5. Select a MIDI track, then address it via `arm track N`; confirm refusal.
6. Begin playback then attempt to arm/disarm a different track; confirm refusal.
7. Begin recording then attempt to disarm; confirm refusal and audio capture continues normally.
8. Check commands via TALK on Windows and via the typed console; verify the recognized transcript.
9. Enter `record vocals`: ensure the existing 15-second confirm-recording gate is still required.
10. Check ordinary `arm track` and `disarm track` still target only the selected audio track.

**Build verification still required:** compile this branch on Windows, run unit tests, then perform AudioBox USB 96 tests. The change is not merged into any release build, and no physical-device test has been performed yet.
