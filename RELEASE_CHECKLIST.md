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
- [ ] `docs/USER_MANUAL.md` refresh — it still opens with "IDW Voice MIDI Studio 4.0" / V5 setup instructions and doesn't reflect the current feature set (Studio Instrument, Audio Lab, retrospective capture, scenes). `README.md` and `docs/V6.x-UPGRADE.md` are current; the manual is not.
- [ ] Editor screenshots inspected — no automated visual check exists; still a manual step before each release.
