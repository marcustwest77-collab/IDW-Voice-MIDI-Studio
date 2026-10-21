# IDW Combined Studio integration beta

This installer brings the preserved V14 DAW and restored Voice MIDI Studio
feature branches into one Windows package. It installs alongside the V14 beta
under a separate application identity. Keep the working V14 beta installed.

## Two workspaces in one package

Start-menu entry **IDW Combined Studio Beta** opens the desktop V14 timeline:
multitrack recording, ten factory skins, audio waveform and MIDI note previews,
per-track vocal FX, lyrics notebook, built-in MIDI instruments, master mix recall,
WAV export, IDW Connect and the local AI approval panel. **BUILD INFO** identifies
the installed revision; **QUICK HELP** summarizes the main controls.

Start-menu entry **Voice MIDI, Tune and Song Studio** opens the restored older
workspace: voice-to-MIDI, beatbox, scale lock, presets, synth/harmony, Tune Studio,
vocal effects, custom skin designer, Song Studio, lyrics/dictation controls,
beat/clip arrangement, takes, stem export, mix checks and recovery controls.
These are source-integrated features, not a claim of hardware validation.

Use one workspace at a time with your audio interface. Close one before opening
the other. They do not share a live transport or automatically exchange projects.
Use WAV or MIDI export/import where supported; keep native projects in their
original workspace. Older Song Studio projects do not become V14 .idwproj files.
A unified timeline and a single audio engine remain future integration work.

## Desktop walkthrough

1. Open **AUDIO DEVICES** and select the microphone/interface input and headphone
   output. Choose your AudioBox input if that is where the microphone is connected.
2. Use **OPEN WAV/AIFF** to import the beat. Add an empty **+ AUDIO TRACK**, click
   its lane header, choose its input, and enable **ARM INPUT**. Check the READY
   message before recording. Enable **PLAY BEAT WHILE RECORDING** for an overdub.
3. Press **RECORD VOCAL**, then **STOP RECORD** to finish. Play the result to hear
   the take with the beat. Check MUTE/SOLO if either part is missing.
4. With transport stopped, click a lane header or clip to select that track.
   Use **RENAME TRACK** for names such as Beat, Lead Vocal, Double and Adlibs;
   **SAVE PROJECT** keeps these names. The vocal FX heading identifies its target. Gentle, Warm and Forward are
   audio-track presets; the amount blends them with the original sound. These
   effects apply to playback/export, not to the recorded source or live monitoring.
5. Audio waveforms show original files, before effects. MIDI note bars show the
   arrangement overview. Select a MIDI track and open **PIANO ROLL** for note
   editing; scroll vertically for lower notes. Lyrics can remain open beside it.
6. Move audio clips by dragging. Resize from the right edge. With a clip selected
   and the timeline focused, Left/Right moves it by a quarter beat;
   Shift+Left/Right changes its length by a quarter beat. Stop audio before editing.
7. Type lyrics in the notebook, then use **SAVE PROJECT**. This saves arrangement,
   lyrics, track FX and master settings, but audio references still depend on their
   files. Use **EXPORT PORTABLE** to copy the project and its audio for transfer.
8. Use **BOUNCE WAV** for an audio mix including built-in MIDI instruments and
   current mix processing. A WAV is an audio export, not an editable project.

Opening another project clears old edit selections, scroll position and pending
AI review. Reopened tracks are disarmed. The AI panel requires a fresh review and
approval for a new action. Connect opens a browser video room; it does not share
DAW audio, synchronize playback or exchange projects.

## What this beta does not yet provide

- Third-party VST3 instruments/effects cannot be loaded into the V14 timeline.
  The bundled IDW VST3 is a plugin for a separate compatible host, not a plugin host.
- Full pitch correction and the complete older Song Studio workflow remain in the
  separate workspace described above.
- The approved design image is a concept. The implemented layout is progressing
  toward it and still needs on-screen Windows validation.

## Focused desktop check when you return

Use a test project or a copy of a session. Confirm the revision in **BUILD INFO**.
Check that a beat and completed take show waveforms; clicking each track updates
its FX target; lyrics remain visible beside the piano roll; expanding/collapsing
the mixer leaves controls accessible; and switching projects clears old selections.
Then save/reopen and compare playback with a bounced WAV. Report the revision,
exact steps, expected result, actual result, and a screenshot if the problem is visual.
Automated build/test success does not replace this real-device check.

## VST3 and optional companions

The Voice MIDI Studio VST3 bundle is included under VST3 in the installation
folder. The installer does not replace any installed plugin or change host paths.
Use a separate host scan location to test this beta plugin.
The Python MCP helper is included under AI and needs a Python interpreter plus
an MCP-capable host; no automatic provider connection is configured.
Audio Lab's packaged Python runtime, optional speech-recognition runtime/models,
and external video services are not bundled or activated by this installer.
Features relying on those companions still require their separate setup.

## Integration provenance and validation

This combines v13-beat-arrangement feature code with the V14 packaging branch,
retaining MidiNoteLedger tracking and reset/flush behavior from v13-midi-safety.
Both sets of CMake tests are built and run by the combined workflow.
BUILD-INFO.json identifies the exact commit and signing status.
This is an integration beta, not a declaration that every historical idea or
optional dependency is complete. Previous audio passes cover the earlier V14
binary only. Plugin-host, workspace-switching, install/rollback and real-device
checks are still needed before a stable combined release.
