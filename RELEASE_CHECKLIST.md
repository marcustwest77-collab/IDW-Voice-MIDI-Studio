# Native-build follow-up

- [x] Installed isolated CMake/pkgconf tools and retried full native configuration
- [ ] Full build: JUCE helper fails on missing `ft2build.h`; system dependency installation blocked by environment permissions
- [x] Prepared existing Windows/macOS workflow for `v12.12-build-readiness`, with required nonempty tests
- [ ] Obtain approval before branch push/CI; no remote change made

# V12.12 integrated build-readiness pass

- [x] 31 direct C++ suites passed against current source (not a full CMake build)
- [x] 52 Python unit tests passed
- [x] Editor, processor and processor-test syntax checked; Linux/macOS build-script shell syntax checked
- [x] Windows package naming derives from CMake version; all scripts reject empty test runs
- [ ] Full native CMake build (local CMake/pkgconf installed; JUCE helper build now blocked by missing FreeType/audio/GUI development dependencies)
- [ ] Linked processor regression, Windows PowerShell execution, installer/host/GUI and AudioBox validation

See [build status and Windows instructions](docs/V12.12-BUILD-READINESS.md).

# V12.12 export check — local candidate

- [x] Mix-check tests passed: silence, peaks, export parity, threshold timing, mute, cancellation and snapshot isolation
- [x] Background export regressions and editor/processor syntax checks passed
- [ ] Native report dialog, progress/cancel, minimum-size/all-skins inspection
- [ ] Windows/macOS build, plugin, installer and AudioBox validation
- [ ] GitHub push only after approval

# V12.11 mix meters — local candidate

- [x] Meter tests passed: post-gain/mute/solo, interval reset, live input, cues, pre-limiter peaks and export isolation
- [x] Recording/export regressions and editor/processor syntax checks passed
- [ ] Native Windows/macOS build, plugin and installer validation
- [ ] GUI: minimum Timeline size, all skins, peak reset, decay and LIMIT visibility
- [ ] AudioBox: meters versus audible mix, monitoring, latency and CPU performance
- [ ] GitHub push only after approval

# V12.10 background export — local candidate

- [x] Worker tests passed: snapshot isolation, progress, cancel, cleanup, output protection and joined shutdown
- [x] Stem regressions and editor/processor syntax checks passed
- [ ] Native Windows/macOS build, installers and plugin validation
- [ ] GUI: progress, cancel, continued editing/playback, panel close and export completion
- [ ] AudioBox playback during export; slow/disk-full destination and application shutdown
- [ ] GitHub push only after approval

# V12.9 stem export — local candidate

- [x] Stem export tests and existing take/clip regressions passed
- [x] Editor/processor JUCE syntax checks passed
- [ ] Windows/macOS native build, installer, plugin and export-menu inspection
- [ ] Import all five files into the target DAW and audition alignment/levels
- [ ] Maximum-duration export and disk-full/interrupted-export manual checks
- [ ] GitHub push only after approval

# V12.8 phrase selection — local candidate

- [x] JUCE-linked phrase, take and clip tests passed; editor/processor syntax checks passed
- [ ] Windows/macOS native build, installer and plugin validation
- [ ] Minimum-size/all-skins menu inspection and phrase workflow
- [ ] AudioBox listening: phrase joins, surrounding audio, undo, Save/Open and export
- [ ] GitHub push only after approval

# V12.7 take selection — local candidate

- [x] JUCE-linked take tests: all four tracks, clip/track mute separation, overlap boundaries, undo/recovery, source sharing, file validation and export parity
- [x] Recording, clip, fade, arrangement and section regressions passed; editor/processor JUCE syntax checks passed
- [ ] Native Windows/macOS build, plugin validation and installer launch
- [ ] Minimum-size and all-skins Timeline inspection: Take options, Clip mute and muted labels
- [ ] AudioBox listening: compare takes, muted backing during recording, Save/Open and exported mix
- [ ] GitHub push only after approval

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

## V11.1 unified lyrics candidate

- [x] Local JUCE-linked song tests: Unicode metadata, v1 migration, corrupt file preservation, lyrics-only projects and existing audio workflow.
- [ ] Full Windows/macOS build and plugin validation for V11.1.
- [ ] Minimum-size and all-skin visual checks of lyrics and beat views.
- [ ] AudioBox USB 96 recording while editing lyrics; close/reopen a saved song.
- [ ] Import/export TXT, cancel dialogs, invalid input and unwritable destination checks.
- [ ] Confirm explicit-save messaging and v2 format compatibility instructions.

## V11.2 native dictation — local candidate

- [x] PCM selection, conversion, wraparound, overflow and concurrent queue tests.
- [x] Worker contract tests with simulated recognizer: final phrase, cancelled loading, duplicate start and changed sample rate.
- [ ] Real Vosk fixture through the native engine on Windows (new CI gate).
- [ ] Windows/macOS compilation, regressions and plugin validation.
- [ ] Native dictation visual checks and AudioBox USB 96 physical input test.
- [ ] Installed and portable Windows runtime/model discovery.
- [ ] CPU stress, simultaneous song recording and dictation, close/reopen and draft limits.
- [ ] macOS compatible Vosk runtime/model supplied manually; not bundled by current packaging.

## V11.3 song recovery — local candidate

- [x] Automated recovery suite: active recording deferral, audio round trip, writing/draft restore, previous-copy fallback, malformed data, instance/document isolation, disk failure and orderly shutdown.
- [ ] Windows/macOS build, plugin validation and all CTest targets.
- [ ] Recovery UI at minimum size, cancel/confirm dialogs and multiple plugin instances.
- [ ] Forced app termination after successful checkpoint, reopen and restore; verify documented in-progress audio limitation.
- [ ] Physical AudioBox checks deferred until hardware is available.

## V11.4 count-in and metronome — local candidate

- [x] Sample-level timing, cancellation, count-in exclusion and click-free recorded/exported audio tests.
- [ ] Windows/macOS builds and full regression/plugin validation.
- [ ] Minimum-size layout in beat, lyrics and dictation views.
- [ ] Headphone listening and AudioBox recording tests deferred until hardware is available.

## V12 four-track studio — local candidate

- [x] Arrangement regression: WAV import, clip timing/trim, mixing, solo/mute, undo/redo, format 3 persistence, malformed-file rejection, export parity and clean backing-assisted recording.
- [x] Existing song, count-in and recovery regressions with the new audio storage.
- [ ] Full Windows/macOS build, plugin validation and real Vosk gate.
- [ ] Timeline minimum-size/all-skins GUI inspection and large-file responsiveness.
- [ ] AudioBox latency/listening/recording checks deferred until hardware is available.

## V12.1 armed-lane recording — local candidate

- [x] All four destinations: count-in cancellation, arm locking, occupied-lane protection, mono/stereo capture, backing exclusion, recording undo/redo, persistence and recovery snapshot isolation.
- [x] Armed-lane monitoring gain/pan stays separate from captured samples.
- [x] Existing song, count-in and recovery regressions; editor/processor syntax checks.
- [ ] Native Windows/macOS build and installer validation; all-skins minimum-size GUI inspection.
- [ ] AudioBox listening/latency/overdub checks when hardware is available.
- [ ] GitHub push approval for this candidate.

## V12.2 multi-clip arrangement — local candidate

- [x] Multi-clip engine tests: split continuity, source sharing, independent imports, overlaps, placement, track mix, export parity, bounded clips, undo and recovery snapshots.
- [x] Format 4 round trip, truncated-file/source-reference rejection and older-format migration.
- [x] Existing armed recording, arrangement, song, count-in and recovery regressions.
- [ ] Full Windows/macOS build/CTest, plugin validation and installer validation.
- [ ] Minimum-size/all-skins Timeline inspection: clip menu, edit cursor, split/copy/move, overlapping selection and track controls.
- [ ] AudioBox listening/latency check, abrupt cut/overlap review and full song save/reopen/export.
- [ ] GitHub push approval for this candidate.

## V12.3 fades and musical grid — local candidate

- [x] Fade sample levels, preserved outer fades on split, crossfade gain, undo, format-5 round trip, malformed/truncated fade rejection, export parity and immutable recovery.
- [x] Snap Off/Bar/Beat/half/quarter subdivisions, non-integer tempo and timeline bounds.
- [x] Existing clip, arrangement/arming, song, count-in and recovery regression suites.
- [ ] Full native Windows/macOS build/CTest, plugin and installer validation.
- [ ] All-skins/minimum-size GUI: fade controls/ramps, grid, selection, cursor and placement.
- [ ] AudioBox listening: short cuts, fades, overlap gain, undo/save/reopen/export.
- [ ] GitHub push approval for this candidate.

## V12.4 sections and loops — local candidate

- [x] Section one-shot and loop sample order, fractional tempo, transport edit protection, marker limits, Unicode names, undo, format 6, malformed/truncated rejection and recovery snapshots.
- [x] Whole-song export after looping, shortened-song bounds, BPM-following markers and recording isolation.
- [x] Repeated drum/metronome loop passes and existing song, count-in, recovery, clip, fade and arrangement/arming tests.
- [ ] Full native Windows/macOS build/CTest, plugin and installer validation.
- [ ] Minimum-size/all-skins Sections/Timeline inspection, marker selection, loop status and Stop.
- [ ] AudioBox listening at loop boundaries, drum/click restart, export and return to recording.
- [ ] GitHub push approval for this candidate.

## V12.5 timeline zoom and seeking — local candidate

- [x] View coordinate mapping, cursor anchoring, scroll bounds, Fit song and invalid zoom input.
- [x] Play cursor, backward seeking, invalid-seek preservation, song-end stop, section/loop bounds, loop wrap after seek, recording/count-in protection and rhythm reset.
- [x] Existing section, count-in and song regressions.
- [ ] Full native Windows/macOS build/CTest, plugin and installer validation.
- [ ] Minimum-size/all-skins zoom/scroll/cursor layout; waveform/fade/grid/marker alignment and overlapping-clip selection.
- [ ] Many-clip GUI responsiveness and AudioBox seek/loop/record listening checks.
- [ ] GitHub push approval for this candidate.

## V12.6 new-clip recording — local candidate

- [x] All-lane cursor recording, original take/snapshot preservation, same-track monitoring, backing exclusion, mono/stereo capture, placement, undo, persistence, count-in cancellation, song end and clip cap.
- [x] Existing arrangement/arming, song, count-in and recovery regressions.
- [ ] Native Windows/macOS build/CTest, plugin and installer validation.
- [ ] Minimum-size/all-skins Record cursor button and armed-track feedback.
- [ ] AudioBox latency/listening, repeated clip recording, export and cancelled-count-in checks.
- [ ] GitHub push approval for this candidate.
