# V13 beat arrangement — local preview

V13 lets verses, hooks and fills use different drums. It extends the existing three-row, 16-step sequencer; it is not yet a complete DAW. No AudioBox is needed for beat programming, file editing or offline export. For playback, select an available computer audio output in Options > Audio/MIDI Settings.

## Make an arrangement

1. Open Song Studio and stop playback/recording. Set BPM and Bars (40–240 BPM, 1–64 bars, maximum 180 seconds).
2. Choose **Base** beside Bars. Set kick, snare and hi-hat steps. Base repeats on bars without custom patterns.
3. Choose a numbered bar and change its steps. That bar now has its own pattern; an asterisk marks it. Later Base edits do not change custom bars.
4. Use **Beat tools > Copy displayed pattern to bars…** to repeat the displayed beat across an inclusive range, for example bars 5–8. This replaces those bars' patterns as one undoable edit.
5. Select a custom bar and choose **Use Base on selected bar** to resume following Base. A custom pattern with all steps off is deliberately silent; it does not revert to Base.
6. Use **Undo beat/track edit** or **Redo beat/track edit** for mistakes. Beat and track edits share the existing eight-entry history. Undoing beats does not undo tempo, song length or master gain changes.
7. Play the song, or use existing section playback/loop controls. Save song to retain the arrangement. Export audio and Check full-song levels use the same arranged drums.

Reducing Bars retains patterns beyond the current song end; increasing Bars brings them back. Tempo changes move drum events and section boundaries but do not time-stretch audio clips. Beat editing is disabled during transport.

## Practice without the AudioBox

Open the standalone app, then Options > Audio/MIDI Settings. Select an available output on your computer and use headphones. You can leave recording for later: program drums, import existing WAV clips in Timeline, arrange sections, write lyrics, save projects and export audio without the AudioBox. If Play is silent, confirm the selected computer output and Beat level; exporting a WAV and playing it separately helps distinguish an audio-device problem from a silent pattern. Avoid enabling microphone monitoring through speakers.

## Project compatibility

Songs with custom bars use format 8 and require V13. V13 also reads formats 1–7; songs without custom bars retain the earliest applicable format. Save a new copy to preserve a project for an older version. Custom patterns are included in recovery snapshots and self-contained song files, not plugin presets.

## Build and run on Windows

Prerequisites: Git, CMake 3.22 or newer, Visual Studio 2022 with Desktop development with C++, and a Windows SDK. Initial configuration requires internet access for JUCE. In a Developer PowerShell, from this repository on branch `v13-beat-arrangement`:

```powershell
cmake -S . -B build-v13 -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build-v13 --config Release
ctest --test-dir build-v13 -C Release --output-on-failure
& ".\build-v13\IDWVoiceMIDIStudio_artefacts\Release\Standalone\IDW Voice MIDI Studio.exe"
```

For the existing complete installer/Audio Lab build requirements, see [build readiness](V12.12-BUILD-READINESS.md). The new guide is included in the packaging workflow. V13 has not yet been pushed or built in native CI; the previously delivered V12.12 installer does not contain these changes.

## Verification

The final local hardening pass also verifies inclusive range copying, out-of-range rejection, stale-dialog rejection, one-step range undo/redo, returning an individual bar to Base, and silent custom bars. Range edits check transport and project revision under the same engine lock.

Automated arrangement tests cover silent Base/custom bars, bar timing, undo/redo, format-8 round trips, malformed/legacy loading, recovery snapshots, retained hidden patterns, section loops, transport edit rejection, and matching playback/mix/stem/level-check output. Existing song, section, recording and take-selection tests also pass. Processor/editor C++ syntax validation passes in the available Linux environment.

Before release on Windows/macOS:

- Run the complete CTest suite and existing packaging/plugin checks.
- Check layout and keyboard interaction at the minimum supported window size.
- Program a Base beat, copy a hook to bars 5–8, add a last-bar fill, save/reopen and listen through transitions.
- Verify undo from both the beat menu and timeline, silent custom bars, range validation, and recovery.
- Export a mix and stems; compare arranged drums with playback and section loops.
- Test device selection with built-in audio now, then AudioBox capture/latency when available.

Local tests do not establish Windows audio performance or installer correctness. No new installer is claimed for this preview.
