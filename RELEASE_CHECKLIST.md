# Release status (current main)

- [x] Windows Release standalone and VST3 compile
- [x] macOS Release standalone, VST3 and AU compile
- [x] Automated C++ processor regressions pass (IDWProcessorRegression, IDWSetupDiagnostics, IDWHarmony, IDWStudioSynth, IDWRetrospective)
- [x] V9.2 editor UI implemented — custom `IDWLookAndFeel` rotary knobs and an IDW logo header embedded via JUCE binary data and drawn centred in the header strip, with the logo's black padding keyed to transparent so it sits on the panel background without a plate
- [x] V9.2 editor UI build and render test confirmed on Windows and macOS at 1120x932 (default) and 1040x922 (minimum) — green CI run [45831350866](https://github.com/marcustwest77-collab/IDW-Voice-MIDI-Studio/actions/runs/45831350866) passed both platform builds, the C++ regressions (including the editor render and bounds checks), pluginval and auval, and uploaded the `editor-renders-Windows` and `editor-renders-macOS` artifacts; re-confirmed after the logo header fix in run [45885683283](https://github.com/marcustwest77-collab/IDW-Voice-MIDI-Studio/actions/runs/45885683283)
- [x] Automated Audio Lab companion regressions pass (47 Python tests, wired into CI on every push/PR to `main`)
- [x] pluginval strictness 5 passes on Windows and macOS
- [x] Audio Unit validated with `auval` on macOS
- [x] Portable Windows and macOS packages produced as CI artifacts (installer + DMG on macOS, installer on Windows)
- [ ] Code signing / notarization — CI packages are explicitly unsigned (see `installer/macos/package.sh`); no Developer ID certificate or notarization credentials are configured yet. Required before public distribution outside direct download.
- [ ] Hands-on microphone and FL Studio listening test — CI validates that the plugin builds and loads, not that it sounds correct with a real voice/mic; still needs a manual pass per release.
- [ ] Broader real-voice drum recognition corpus — beatbox-to-drum detection is only covered by synthetic/unit-level tests, not a varied real-voice corpus.
- [ ] `docs/USER_MANUAL.md` refresh — it still opens with "IDW Voice MIDI Studio 4.0" / V5 setup instructions and doesn't reflect the current feature set (Studio Instrument, Audio Lab, retrospective capture, scenes). `README.md` and `docs/V6.x-UPGRADE.md` are current; the manual is not.
- [x] Editor screenshots visually inspected — the renders from green CI run [46146683825](https://github.com/marcustwest77-collab/IDW-Voice-MIDI-Studio/actions/runs/46146683825) were reviewed on Windows and macOS at 1120x932 (`IDW-V6-Preview`, Performance) and 1040x922 (`IDW-V6-Minimum`), plus Studio, Instrument and Connection at 1040x922: the logo is centred in the header in every view (centre 519.5 px in the 1040-wide renders, 559.5 px in the 1120-wide render), and after the edge-keying change the logo header shows no dark plate and no hairline edge — the whole tile border renders as panel colour (0 of 184 border pixels differ from `(11,14,20)` by more than 2/255 at either size on either platform). Re-check the `editor-renders-*` artifacts after any further UI change.
