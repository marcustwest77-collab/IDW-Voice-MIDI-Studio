# V12 — first four-track studio candidate

V12 adds a native audio timeline and mixer foundation to Song Studio. This is local source, not a validated installer or a complete DAW. It includes the local dictation, recovery and count-in milestones.

## What is available

| Audio lane | Initial role | Source |
| --- | --- | --- |
| 1 | Lead vocal | Existing recorded take, a new Record take, or an imported WAV |
| 2 | Beat / backing | Imported WAV |
| 3 | Double | Imported WAV |
| 4 | Ad-lib | Imported WAV |

Each lane holds one stereo clip (mono imports are duplicated to both channels). These are role labels, not restrictions on file content. The existing synthesized drum sequencer remains separate and can play alongside all four lanes.

The native timeline displays a waveform overview, clip position and the shared playhead. Select a lane to import, remove, move or trim its clip and set gain, pan, mute and solo. Clip movement and trimming use numeric/sliding controls followed by **Apply track edit**; there is no direct waveform dragging, splitting, duplication or zoom yet.

## First session without a microphone

1. Open **Song Studio**. Set BPM and Bars. Song length remains 1–64 bars with a 180-second maximum. Imported audio is not time-stretched when tempo changes.
2. Open **Timeline**. Select **2 / Beat / backing**, then **Import WAV…**. Use a mono/stereo WAV at 8–192 kHz, up to 180 seconds. Save or cancel when asked before replacing an existing clip.
3. Import prepared vocals into Lead, Double and Ad-lib as needed. Import never changes your source files.
4. Select a lane, set **Start / s** for its timeline position, and **Trim in / s** / **Trim out / s** for the audible range inside its source file. Click **Apply track edit**. Trimming is nondestructive: the entire source remains in the project.
5. Set Gain and Pan, or Mute/Solo, then Apply. Pan is a stereo balance control: center preserves both channels; left/right attenuate the opposite side. Mute takes precedence over solo.
6. Press **Play song**. All audible lanes follow the same sample-clock transport. The playhead and waveforms show where your clips sit. Editing/importing requires stopped transport.
7. Set Bars long enough to include the clips. Audio beyond the chosen song end is excluded from playback/export without deleting its source data. Clips placed entirely past the end will be silent.
8. If you imported a beat and do not want the synthesized drums, return to Beat controls and turn **Beat level** down. **Audio tracks level** is the overall level for all four audio lanes.
9. **Save song** embeds all audio. **Export mix WAV** renders the same arrangement/mix at 48 kHz, 24-bit stereo, excluding metronome/count-in cues.

Soloing an audio lane also silences the separate synthesized drum pattern. More than one lane can be soloed. A soloed empty or muted lane can therefore leave the mix silent. Mono host output is a downmix of the left/right audio mix.

## Recording with backing tracks

**Record take** still records only the lead lane from bar 1. It refuses to overwrite a nonempty lead lane. Save the project, then Remove clip or Discard take to clear that lane before recording again. Count-in works as before.

The other three imported lanes play during the take. IDW records the processed microphone/instrument input before mixing in the backing lanes, drum sequencer or metronome. Use headphones to prevent acoustic bleed into the microphone. Track settings affect monitoring/playback, while the captured lead audio remains available for subsequent edits. Recording resets the lead clip's placement/trim to start at bar 1.

Track arming for recording directly into Double or Ad-lib, multitrack live capture, punch-in, loop recording and take comping are later phases. Physical AudioBox testing remains deferred.

## Undo, recovery and memory

**Undo track edit** / **Redo track edit** cover imports, removals, applied track settings and lead take changes. This is track-edit history, not global undo for lyrics, drum patterns or tempo. Up to eight recent undo states are retained, with older states dropped when the retained audio would exceed 512 MiB. History is not saved across app restart or project opening.

Completed audio buffers are shared without copying between undo states and recovery snapshots. A new recording gets a new buffer, so it cannot overwrite audio retained by an older checkpoint. The current project has a 512 MiB decoded-audio budget. Temporary import/loading buffers and existing snapshots can increase the process's actual memory use above that budget. Large import/save/export operations can occupy the interface while stopped.

V11.3 recovery now includes all four lanes and their clip/mixer settings. Writing is still checkpointed separately. In-progress audio recording remains outside recovery protection.

## Project compatibility

- V12 reads original song formats 1 and 2. Their recorded audio appears in the Lead lane; the other lanes start empty.
- Projects containing additional audio lanes or changed timeline/mixer settings use **format 3**. V11.1–V11.4 cannot open them. Save a new copy if you need to retain an older-version project.
- A project using only the unarranged original lead take can still save as format 2.
- Saved projects embed their source audio; reopening does not depend on the original WAV paths.
- Invalid/truncated files are rejected before replacing the current song.

## Build and validation

Use the [Windows source-build instructions](V11.1-UNIFIED-LYRICS.md) with a fresh `build-v12` directory. The application version is 12.0.0. Build all targets and run CTest. New target: `IDWSongArrangement`.

Locally verified: mono/stereo-ready import, mixed source sample rates, placement/trim boundaries, gain/pan/mute/solo, undo/redo, embedded format 3 round trip, corrupted-file preservation, recovery snapshot isolation, live/export parity, and backing-assisted recording without backing audio in the recorded lead. The existing Song Studio, count-in and recovery test executables also pass against the new engine. Editor/processor source passes JUCE-header syntax checks.

Remaining gates: full Windows/macOS builds, plugin validation, real Vosk speech gate, timeline GUI inspection at minimum size/all skins, large-project responsiveness and AudioBox recording/listening. No new GitHub push or installer is implied by these local results.

## Next DAW phases

Multiple clips per lane; direct drag/split/duplicate and clip fades; recording into any armed lane; loop/punch recording and take comping; a dedicated mixer with per-track effects/sends; MIDI piano roll and sample-based beat editing; and dedicated stem export. Current sample-rate conversion uses linear interpolation, and trim boundaries do not yet have automatic fades. This milestone establishes the shared audio arrangement rather than claiming those later features are complete.
