# IDW Voice MIDI Studio 4.1 — source upgrade

Open **START-HERE.html** for build, installation, routing, troubleshooting and test instructions.

**No new EXE or VST3 is included in this source package.** Run BUILD-WINDOWS.cmd on a configured Windows development computer to build and test them. Read docs/VALIDATION-4.1.md for current verification limits and docs/CHANGES-4.1.md for changes.

## Original V4 documentation (historical)

# IDW Voice MIDI Studio 4.0

Windows standalone and VST3 voice-to-MIDI, built with JUCE 9.0.2. This upgrade retains the original plugin identifier and IDW_V9 session tree for compatibility.

## What changed

- Fixed 5 ms analysis hops independent of the host audio buffer; bounded YIN analysis and time-based smoothing, note confirmation and release.
- Strict scale mode keeps pitch bends centered; Natural vibrato retains up to 45 cents of local expression.
- Mono/stereo input support, selectable analysis channel, independent microphone monitoring and a simple sine preview instrument.
- Test Note, Panic, live pitch history, MIDI event counts and callback load display.
- MPE note-offs use the channel that originally started the note. Channel 10 is reserved for drums. Pitch-bend range is sent using RPN.
- Channel-aware MIDI Learn handles all CC events in a block. Mappings persist with sessions and user presets.
- Eight trainable drum pads using five local spectral examples per pad; velocity-sensitive hits and 40 ms MIDI drum durations.
- MIDI recording up to ten minutes with export of tempo, notes, bends and CCs. Export explicitly closes held notes.
- Standalone input is enabled for analysis while direct microphone monitoring defaults off at startup.
- UI-to-audio communication uses atomic commands / a bounded single-producer queue; no disk writes or training allocations in the audio callback.

See [the manual](docs/USER_MANUAL.md), [FL Studio setup](docs/FL_STUDIO_SETUP.md), and [the validation report](docs/VALIDATION.md).

## Build

Requirements: CMake 3.22+, a C++17 compiler, and JUCE 9.0.2. On Windows use Visual Studio 2022 Build Tools with the Desktop C++ workload and Windows SDK.

```powershell
./scripts/build-windows.ps1 -JucePath 'C:/path/to/JUCE'
```

Alternatively:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIDW_JUCE_PATH=/path/to/JUCE
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```

If no local JUCE path is supplied, CMake fetches the pinned 9.0.2 tag. Windows uses a static MSVC runtime so the portable app does not require installing a new Visual C++ runtime. macOS source/build recipes are included but this delivery was built on Windows only.

## Compatibility and scope

The VST3 identity is unchanged: install one version at a time and back up important DAW sessions before upgrading. Existing settings load; old presets without drum profiles or MIDI mappings get empty maps. Version 3 exposed a latencyMs parameter that did not implement delay; its ID is retained for session compatibility, but it is not presented as a working latency control.

This is monophonic voice tracking, not polyphonic transcription. Drum recognition uses local spectral templates rather than a neural model. The MIDI take is editor-local: export before closing the editor. Tempo is fixed per take. No cloud service or API key is needed.

JUCE and its bundled components retain their upstream licenses. This source package does not change the project's existing licensing terms. Build tools are not included.
