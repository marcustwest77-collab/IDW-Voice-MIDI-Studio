# V11 Song Studio — local candidate

- [x] Sample-clock sequencer timing tests passed locally
- [x] JUCE-linked recording, project persistence, corrupt-file rejection, resampling and WAV export tests passed locally
- [ ] Full Windows/macOS V11 build and plugin validation
- [ ] Native GUI render and minimum-size inspection
- [ ] AudioBox USB 96 vocal recording and exported-mix listening tests
- [ ] Save/open, overwrite cancellation and disk-failure manual checks
- [ ] No V11 installer produced yet; no V11 GitHub push performed

## Carried-forward V10.7 checklist

V10.7 Custom Skin Designer release status (local candidate)

- [x] V10.6 release candidate compiled, validated and packaged successfully on Windows and macOS
- [ ] Windows/macOS V10.7 CI build and processor regressions pass (requires approved branch push)
- [x] Baseline V10.6 Audio Lab contract regressions passed (52 Python tests; Audio Lab unchanged in V10.7)
- [x] V10.7 colour parser/mapping and V10.6 palette tests pass locally with warnings treated as errors
- [x] Editor, processor and processor tests pass JUCE-header syntax checking
- [ ] Execute new processor state round-trip and legacy custom-skin regressions in a linked build
- [ ] Manual designer save/load, malformed file, recovery and project-reopen checks
- [x] Official IDW logo source, embedded resources, multi-resolution Windows icon and installer artwork validated locally
- [ ] Packaged Audio Lab real-model gate passes Tune Studio DSP and Basic Pitch (requires Windows CI)
- [ ] V10.7 pluginval strictness 5 and macOS `auval` pass (requires CI)
- [ ] V10.7 portable Windows/macOS packages and installers produced (requires CI)
- [ ] Code signing / notarization — CI packages are explicitly unsigned (see `installer/macos/package.sh`); no Developer ID certificate or notarization credentials are configured yet. Required before public distribution outside direct download.
- [ ] Hands-on microphone and FL Studio listening test — CI validates that the plugin builds and loads, not that it sounds correct with a real voice/mic; still needs a manual pass per release.
- [ ] Broader real-voice drum recognition corpus — beatbox-to-drum detection is only covered by synthetic/unit-level tests, not a varied real-voice corpus.
- [x] `docs/USER_MANUAL.md` reflects the six V10.6 skins plus Vocal FX, safety/quality controls, Tune Studio, Pro Tools, routing, lyrics, synth and take features.
- [ ] Manual visual pass confirms all six skins in Performance, Studio Controls, Vocal FX, Instrument and Connection panels.
- [ ] Automated editor render/bounds coverage produces V10 Performance, Studio, Instrument, Vocal FX and Connection Center previews (requires CI compile).
- [ ] Real voice listening test for Natural/Tight/Hard presets and Pro Tools import alignment.
- [ ] Human screenshot inspection and Windows display-scaling pass — automation catches empty/out-of-bounds controls, not poor visual hierarchy at every DPI.
