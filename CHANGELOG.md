# Changelog

## 0.8.2 Release Candidate

- Rebuilt the plugin editor as a black/gold IDW performance control surface.
- Added live microphone level metering and one-click microphone gate calibration.
- Added direct UI controls for gate, pitch confidence, bend range, beatbox sensitivity and MIDI latency.
- Added UI controls for scale lock, root, custom 12-note mask, Beatbox, Gesture CC and MPE.
- Added preset browser/load workflow while retaining preset save.
- Corrected pitch bend so it is calculated relative to the final emitted/scale-locked MIDI note.
- Reset pitch wheel cleanly on note changes and note-off.
- Made voice note release timing independent of audio buffer size.
- Mapped voice level to note velocity.
- Improved beatbox classification with onset, zero-crossing, brightness and crest features.
- Made beatbox cooldown time sample-rate based rather than block based.
- Reserved MIDI channel 10 for beatbox drums to avoid MPE channel collisions.
- Expanded bus-layout validation to mono input with mono or stereo output.
- Versioned Windows installer, macOS PKG and macOS DMG outputs.
- Added tag-driven GitHub Release publishing after Windows/macOS validation succeeds.

## 0.8.1

- Added automatic JUCE fetching for CI/local builds.
- Enabled VST3 + Standalone on Windows/macOS and AU on macOS.
- Added pluginval and auval validation.
- Added Windows/macOS installer packaging.
