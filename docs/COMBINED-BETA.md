# IDW Combined Studio integration beta

This installer brings the preserved V14 DAW and restored Voice MIDI Studio
feature branches into one Windows package. It installs alongside the V14 beta
under a separate application identity. Keep the working V14 beta installed.

## Two workspaces in one package

Start-menu entry **IDW Combined Studio Beta** opens V14: Neo Cyan, Royal Gold,
Emerald Circuit, Purple Night, custom skins, multitrack recording, timeline,
WAV export, IDW Connect and the local AI approval panel.

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
