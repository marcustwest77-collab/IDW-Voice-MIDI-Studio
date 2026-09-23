# V5 validation status

Local checks performed:
- Compiled and ran the independent HarmonyTests C++17 executable: known chords, disabled/invalid inputs and 6,144 combinations of scale, root, pitch and voicing passed.
- Compiled and ran SetupDiagnosticsTests: ten scenarios passed.
- Syntax checks of editor, processor, preset manager and processor tests against the available JUCE headers: see docs/V5-CHECKS.txt.

Full Windows compilation and executable tests are pending. No V5 binary or live microphone/DAW listening result is claimed.

Prepared regression coverage (must execute on Windows):
- Existing V4 pitch, sample-rate/block-size timing, scales, MPE, Panic, MIDI learn/state, drums, preview/stereo, capture and export.
- V5 chord/bass channel allocation and release, MPE exclusion and range filtering.
- Legacy V4 state migration.
- Voice-profile disk roundtrip and isolation from musical settings.
- Recording survives editor destruction; MIDI recovery and tempo; failed recovery preserves current take.
- Performance/default/minimum and Studio view rendering and control bounds.

Windows workflow runs these and pluginval strictness 5 before uploading the candidate. It includes rendered previews and logs in the download.

Live checks still required after build: real microphone, chosen VST instrument's channel filtering, synth patch/gain/output, audio buffer performance, saved FL Studio session roundtrip and a recovery test after host restart. Backups depend on a responsive message thread and writable local storage; checkpoint interval is about five seconds, not a zero-loss guarantee.
