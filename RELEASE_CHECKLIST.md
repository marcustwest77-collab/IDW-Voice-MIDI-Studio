# Release status (current main)

- [x] Windows Release standalone and VST3 compile
- [x] macOS Release standalone, VST3 and AU compile
- [x] Automated C++ processor regressions pass (IDWProcessorRegression, IDWSetupDiagnostics, IDWHarmony, IDWStudioSynth, IDWRetrospective)
- [x] Automated Audio Lab companion regressions pass (47 Python tests, wired into CI on every push/PR to `main`)
- [x] pluginval strictness 5 passes on Windows and macOS
- [x] Audio Unit validated with `auval` on macOS
- [x] Portable Windows and macOS packages produced as CI artifacts (installer + DMG on macOS, installer on Windows)
- [ ] Code signing / notarization — CI packages are explicitly unsigned (see `installer/macos/package.sh`); no Developer ID certificate or notarization credentials are configured yet. Required before public distribution outside direct download.
- [ ] Hands-on microphone and FL Studio listening test — CI validates that the plugin builds and loads, not that it sounds correct with a real voice/mic; still needs a manual pass per release.
- [ ] Broader real-voice drum recognition corpus — beatbox-to-drum detection is only covered by synthetic/unit-level tests, not a varied real-voice corpus.
- [x] `docs/USER_MANUAL.md` reflects the current Studio Instrument, Audio Lab, retrospective capture, scenes and V9.2 DAW Connection Center.
- [x] Automated editor render/bounds coverage produces Performance, minimum-size, Studio, Instrument and Connection Center previews during the C++ regression suite.
- [ ] Human screenshot inspection and Windows display-scaling pass — automation catches empty/out-of-bounds controls, not poor visual hierarchy at every DPI.
