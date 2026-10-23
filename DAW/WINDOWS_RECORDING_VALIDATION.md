# IDW Neo Studio V14 — Windows recording validation

## Verified recording milestone (2026-10-22)

User-reported real-world Windows tests on the V14 branch passed:
- USB microphone recording
- Vocal WAV file creation
- Automatic take import
- Audible recorded playback
- Recording failure handling
- Occupied audio track protection
- Project save and reopen

The user reports that GitHub Actions run #665 for commit `e87026e53ccd20be1949b0da537e740ac5a1eb76` passed.

**Status: recording reliability verified for the tested Windows configuration.** This is not a blanket certification for all audio devices or operating systems.

## Next milestone — Recording Preflight

Implemented in `DAW/Main.cpp` via the **PREFLIGHT** button:
- Inspect selected track type, arm state, destination occupancy
- Check active audio device, enabled input, output and sample rate
- Confirm the recordings directory is available and warn if free storage is under 100 MiB
- Provide a consolidated PASS / ACTION NEEDED dialog
- Refuse preflight while playback, recording, export or calibration is active

The preflight is a configuration check, **not** a live microphone test, folder write test or audible-output test.

## Required validation of this new milestone

1. Confirm Windows Actions build passes on the preflight commit.
2. Click PREFLIGHT with a working USB microphone, empty armed audio track and sufficient disk space; verify PASS.
3. Disarm the track; verify ACTION NEEDED.
4. Load a clip into the selected audio track; verify occupied-track failure.
5. Disable its input in AUDIO DEVICES; verify input failure.
6. Start playback and click PREFLIGHT; verify busy-state warning.
7. Confirm ordinary RECORD VOCAL still produces a WAV, auto-imports and plays audibly.
8. Save, close and reopen the project to check regression.

## Roadmap after preflight

1. Nondestructive multitrack editing: clip split, trim, fades and grid snap.
2. Neo Mix Console: channel EQ, compression, reverb and preset chains.

## Preflight verified (2026-10-22)

User reports Windows beta from GitHub Actions run #667 passed:
- PREFLIGHT PASS when setup was ready
- PREFLIGHT ACTION NEEDED for invalid setup
- Recording still works after the change

**Status: recording preflight verified in the user's Windows test environment.**

## Professional multitrack editing — milestone 1 (in progress)

Initial commits implement source-preserving split/left-edge trim/right-edge trim operations in
`SessionModel.h`, keyboard editing commands, and a model regression test registered with CTest.

Editing shortcuts while stopped, after selecting a clip:
- Click the timeline ruler to set the playhead
- `S` splits the selected clip at the playhead
- `[` trims the start forward by a quarter-beat
- `]` trims the end backward by a quarter-beat
- `Ctrl+Z` undo, `Ctrl+Y` redo, `Ctrl+Shift+Z` redo

**Known limitation:** Existing DAW audio playback and offline bounce currently render the
first clip per audio track only. Multi-clip audible playback after splitting is **not verified**
and must be implemented before this milestone is considered complete.
Undo/redo is currently scoped to split and keyboard trim operations. Other old clip moves
and drags are not covered by the new history yet.

### Next QA

1. Confirm Actions compilation and IDWDAWClipEditing test.
2. Select an existing clip, set the stopped playhead within it, press S; verify two clip rectangles.
3. Select the right clip and press [; verify its start moves forward while the source WAV remains unchanged.
4. Press ]; verify its end moves back.
5. Use Ctrl+Z, Ctrl+Y and Ctrl+Shift+Z, checking clip count, boundaries and offsets.
6. Save and reopen the project; verify split segments and source offsets persist.
7. Verify actions at clip boundaries are ignored safely.
8. Test first and second split segments for audio continuity **after multi-clip playback support is implemented**.
