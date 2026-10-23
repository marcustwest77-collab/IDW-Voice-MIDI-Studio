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
