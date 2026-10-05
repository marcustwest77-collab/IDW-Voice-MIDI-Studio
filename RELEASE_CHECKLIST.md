# V10.5 release status (local candidate)

- [x] Previous V9.2 baseline compiled on Windows and macOS
- [ ] Windows/macOS V10.5 CI build and processor regressions pass (requires approved branch push)
- [x] Automated Audio Lab contract regressions pass locally (52 Python tests)
- [x] Eleven native C++ algorithm suites pass locally with warnings treated as errors, including V10.5 Clip Guard, confidence-gate and CPU-quality coverage
- [x] Official IDW logo source, embedded resources, multi-resolution Windows icon and installer artwork validated locally
- [ ] Packaged Audio Lab real-model gate passes Tune Studio DSP and Basic Pitch (requires Windows CI)
- [ ] V10.5 pluginval strictness 5 and macOS `auval` pass (requires CI)
- [ ] V10.5 portable Windows/macOS packages and installers produced (requires CI)
- [ ] Code signing / notarization — CI packages are explicitly unsigned (see `installer/macos/package.sh`); no Developer ID certificate or notarization credentials are configured yet. Required before public distribution outside direct download.
- [ ] Hands-on microphone and FL Studio listening test — CI validates that the plugin builds and loads, not that it sounds correct with a real voice/mic; still needs a manual pass per release.
- [ ] Broader real-voice drum recognition corpus — beatbox-to-drum detection is only covered by synthetic/unit-level tests, not a varied real-voice corpus.
- [x] `docs/USER_MANUAL.md` reflects Vocal FX, V10.5 safety/quality controls, Tune Studio, Pro Tools, routing, lyrics, synth and take features.
- [ ] Automated editor render/bounds coverage produces V10 Performance, Studio, Instrument, Vocal FX and Connection Center previews (requires CI compile).
- [ ] Real voice listening test for Natural/Tight/Hard presets and Pro Tools import alignment.
- [ ] Human screenshot inspection and Windows display-scaling pass — automation catches empty/out-of-bounds controls, not poor visual hierarchy at every DPI.
