# Version 3 release notes

Version 3 is the next runnable development build of IDW Voice MIDI Studio. It is
based on the repository's V9.1 main branch and preserves the existing APVTS state
identifier so older sessions and presets remain compatible.

## Improvements

- Noise calibration now measures 30 UI samples over one second and uses the peak
  ambient level. This avoids setting a bad gate from a single quiet snapshot.
- Pitch is lightly smoothed in semitone space while large jumps reset immediately.
- Note changes require two consecutive analysis frames, reducing boundary chatter.
- Note release is based on 75 ms of silence instead of a fixed number of audio
  blocks, making behavior consistent at 64, 128, 256, and larger buffer sizes.
- Gesture CC messages are rate-limited to reduce unnecessary MIDI traffic.
- Product/build version is now 3.0.0; the UI and saved preset names identify V3.

## Build and run

Requirements: CMake 3.22+, a C++17 compiler, Git, and platform audio development
packages. JUCE 9.0.2 is fetched automatically unless `ThirdParty/JUCE` exists.

### Linux

```bash
./scripts/build-linux.sh
```

### macOS

```bash
./scripts/build-macos.sh
```

### Windows (PowerShell)

```powershell
./scripts/build-windows.ps1
```

Run the Standalone target from the generated build artifacts, or install/copy the
VST3 into the system VST3 folder and rescan in the DAW. In FL Studio, match the
IDW wrapper MIDI output port to the destination instrument's MIDI input port.
For exact routing and troubleshooting, see [`FL_STUDIO_SETUP.md`](FL_STUDIO_SETUP.md).

## Version 3 smoke test

1. Launch Standalone or load the VST3 and confirm microphone input moves INPUT.
2. Stay quiet, press CALIBRATE NOISE, and confirm the one-second measuring status.
3. Load Clean Vocal, route MIDI, and sustain several notes around semitone borders.
   Confirm notes do not rapidly alternate.
4. Repeat at 64, 128, and 256 sample buffers. Stop singing and confirm note-off is
   consistent rather than changing significantly with buffer size.
5. Match the synth bend range to IDW, sing slides, and confirm smooth bends.
6. Enable SCALE LOCK and verify every output note belongs to the selected mask.
7. Load Beatbox Drums and verify kick 36, snare 38, and hat 42 on channel 10.
8. Test MIDI Learn, user preset save/load, MPE with a compatible synth, and plugin
   state recall after closing and reopening the DAW session.

## Known limits

- Pitch/beatbox accuracy still depends on microphone quality, room noise, and the
  monophonic source; this is not a trained Dubler model.
- Windows/macOS installers must be produced and signed on those operating systems.
- Final hardware/DAW validation is still required before a public release.
