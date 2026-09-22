# V4.1 setup and diagnostics upgrade

- Live guidance separates stopped audio, silent input, clipping, disabled tracking, low level, uncertain pitch, training and accepted notes.
- Selected-input peak meter data decays over 500 ms so short peaks can be observed by the UI.
- Sample-rate, block-size and audio callback telemetry uses atomics and does not query a device from the audio thread.
- Copy diagnostics includes parameter values, live signal readings and generated MIDI counts. No audio or personal paths are included.
- Calibration leaves the previous gate untouched if audio stops, the input is digital silence or the peak reaches clipping level.
- Windows build launcher works from its own folder, can locate VS2022 CMake, gates packaging on tests, and creates a dated portable release.
- Detailed visual setup guide distinguishes routing ports from MIDI channels and native FL instruments from VST instruments.

Preserved: plugin identity, existing parameter identifiers and state type, standalone V4 settings filename, melody, scales, MPE, trained drum profiles, presets, MIDI learn, recording/export, preview and Panic. Back up sessions and presets before replacing a build; binary compatibility still requires host testing.

Scope: this is V4.1, an incremental setup/reliability upgrade. It does not add polyphonic recognition or confirm that the DAW received generated MIDI.
