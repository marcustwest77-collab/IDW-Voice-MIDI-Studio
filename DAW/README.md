# IDW Voice MIDI Studio — V14 development

Experimental standalone JUCE workstation (separate from the V13 voice-to-MIDI plugin).

Current code includes multitrack audio playback, basic vocal capture, session persistence, offline WAV bounce, master EQ, resampling, and automated tests. These are **not** yet validated on all physical audio devices.

## IDW Connect — two-person video (first integration)

The **IDW CONNECT** button in the DAW opens a collaboration panel:
1. Choose **CREATE PRIVATE CODE** to generate a random room identifier.
2. Choose **COPY INVITE** and share it directly with one trusted collaborator.
3. Both participants choose **OPEN VIDEO ROOM**, or the recipient opens the supplied link.

The video meeting runs in the default browser through the third-party Jitsi Meet service. Camera and microphone permissions are handled by the browser. Do not share room codes publicly. Room identifiers are not passwords; meeting admission behavior and data handling depend on the external service.

**Scope and limitations:** IDW Connect currently provides invitation and launch UI only. It does not embed a camera view inside JUCE, enforce a two-participant maximum, stream DAW output, synchronize transports, transmit recordings, share project files, or provide end-to-end validated recording collaboration. Do not enable speaker-to-microphone monitoring without headphones: feedback or echo may occur. Production deployment should add access controls, service configuration, tests, and a privacy review.

## Build

Configure with `-DIDW_BUILD_DAW=ON` and compile `IDWDAW`. The V14 DAW workflow runs Windows/macOS build and regression tests on branch `v14-daw-foundation`.

## Remaining DAW priorities

- Test hardware microphone/monitor timing and track-recording latency.
- Test longer sessions and concurrent recording/export use cases.
- Improve timeline editing, project reliability, MIDI/audio routing, and plugin hosting.
- Add video embedded view, secure admission and synchronized collaboration as future, separately tested milestones.

## AI assistant integration — local MCP foundation

V14 now includes `DAW/ai/idw_mcp_server.py`, a **local stdio MCP server** using only Python's standard library. It works with MCP-capable AI host applications, where local MCP connections are supported. It is **not** a direct in-DAW ChatGPT/Claude panel yet and does not need an OpenAI or Anthropic API key.

For an MCP host supporting local stdio servers, configure a command equivalent to:

```json
{
  "mcpServers": {
    "idw-daw": {
      "command": "python3",
      "args": ["/ABSOLUTE/PATH/TO/REPO/DAW/ai/idw_mcp_server.py"],
      "env": {
        "IDW_PROJECT_FILE": "/ABSOLUTE/PATH/TO/SAVED_IDW_PROJECT.json"
      }
    }
  }
}
```

On Windows use the appropriate Python executable and native file paths; host-specific configuration varies.

Available tools:
- `idw_project_status` — read BPM, meter, track count from saved V14 project.
- `idw_list_tracks` — list track names/types/mix settings without audio file paths.
- `idw_preview_tempo` — validate prospective BPM.
- `idw_preview_track` — propose an audio or MIDI track.
- `idw_preview_mix` — validate track gain (0–2) and pan (-1 to +1) against a saved project, without applying changes.
- `idw_analyze_project` — read-only diagnostics for empty audio tracks, mute/solo conflicts and invalid clip timing, without exposing audio file paths.

These operate on an explicit saved **snapshot**, not the live JUCE timeline. **Read-only and preview-only:** nothing changes in the DAW. No audio files are uploaded by this server. Track names and project metadata can be shared with the selected AI provider, so use only trusted AI applications.

Run `python3 DAW/ai/test_mcp_server.py` for protocol and no-write safety tests.

Later milestones: live DAW command bridge with explicit approval, embedded AI panel, provider credentials and secure external connectivity. These are not yet implemented.

## AI command approval foundation

The new `DAW/AIProposal.h` defines a strict `idw-ai-proposal-v1` JSON contract for three non-destructive actions: set tempo, add audio track, or add MIDI track. It rejects extra keys, unknown action types, and out-of-range BPM before a caller can apply anything.

Example proposed change (NOT applied automatically):

```json
{"format":"idw-ai-proposal-v1","action":{"type":"set_tempo","bpm":145}}
```

The JUCE parser and action helper are covered by `IDWDAWPersistenceTests`. They are **not yet connected to an interactive approval window, live MCP bridge, or in-DAW AI chat**. Continue to use the DAW's manual controls. No recording or project destruction is permitted through this proposal interface.

## IDW AI Studio approval panel (V14 experimental)

Choose **IDW AI STUDIO** in the DAW toolbar. Paste an `idw-ai-proposal-v1` JSON action, choose **REVIEW PROPOSAL**, inspect the action description, then click **APPROVE & APPLY** once. Supported actions: set BPM (20–300), add one audio track, add one MIDI track. Requests are rejected while playback, recording, or WAV export is active. Unknown commands and extra parameters are rejected. This panel is **local and manual**: there is still no direct MCP-to-running-DAW connection and no embedded ChatGPT or Claude conversation. The separate MCP server reads saved-project snapshots and produces previews; its output is not auto-executed.

### AI preview handoff (manual)

The MCP tools `idw_preview_tempo` and `idw_preview_track` now return `approval_json`. Copy only that JSON object into the DAW's **IDW AI STUDIO** panel, choose **REVIEW PROPOSAL**, then **APPROVE & APPLY**. The DAW validates the JSON again and will not apply it without a click. This is a local/manual handoff, **not live MCP remote control**. Mix changes still return previews only and cannot be applied through the approval panel yet.

The **PASTE JSON** button imports a proposal from the system clipboard. Pasting disables approval until **REVIEW PROPOSAL** is selected again, and changes still require an explicit **APPROVE & APPLY** click.

The MCP `idw_preview_mix` tool now returns compatible `approval_json` for `set_track_mix`. The AI STUDIO panel can review and apply track gain (0–2) and pan (-1 to 1) to an existing track after a user click. Track indices are zero-based; changes are rejected while recording, playing, or exporting. No network control or automatic edits have been enabled.

AI approval safety: immediately before execution, IDW AI STUDIO reparses the editor JSON and checks that the action and all parameters still match the previously reviewed proposal. Changed or invalid proposals must be reviewed again.

### Current-session status for AI assistants

The **COPY STATUS** button in IDW AI STUDIO copies a sanitized snapshot of the **running DAW** to the system clipboard, including tempo, transport/recording flags, and track mix settings/counts. It deliberately excludes media locations and audio content. Copying is opt-in; no data is uploaded automatically. Pasting the status into a third-party AI application shares its metadata with that provider. This is not yet a live streaming MCP connection.

For **COPY STATUS**, V14 now presents a confirmation dialog explaining that track names and mixer metadata will be placed on the system clipboard; **CANCEL** leaves the clipboard unchanged. This remains a manual opt-in transfer rather than an automatic AI connection.

The AI approval panel now refuses pasted proposals larger than 4 KB before parsing; it also enforces this size limit during manual review. This is a defensive bound, not a replacement for explicit user approval.

### Importing AI proposals from files

The IDW AI STUDIO panel now supports **IMPORT JSON**. It opens a local file picker and reads a JSON proposal of at most 4 KB. The user must still click **REVIEW PROPOSAL** and **APPROVE & APPLY**; importing cannot execute a command. This is a user-initiated file handoff, not a running MCP socket or automatic message bridge.

### Opt-in session snapshot handoff to MCP

**SAVE STATUS** inside IDW AI STUDIO asks for confirmation, then writes a sanitized JSON snapshot to a file you select. In your local MCP configuration, set `IDW_STATUS_FILE` to that saved file and call `idw_session_snapshot` from the AI client. This tool reads only the explicitly exported snapshot and excludes source paths even when a malformed file includes them. Export again to refresh the snapshot; there is no file watcher or live connection. Track names and mix information can still be sensitive, so save and share only when appropriate. This does not authorize AI to apply commands automatically.

### MCP-to-DAW proposal inbox (opt-in staging)

Set `IDW_PROPOSAL_INBOX` to a dedicated absolute filename ending in `.json`, in an existing directory. An MCP client can call `idw_stage_proposal` to write **one** validated tempo, track-addition, or track-mix proposal to that filename. The server refuses to overwrite an existing inbox file, never executes the proposal, and requires explicit configuration before writing. In the DAW, choose **IDW AI STUDIO → IMPORT JSON**, select the staged file, then **REVIEW PROPOSAL → APPROVE & APPLY**. Delete or archive the inbox file after handling it to allow a new proposal. This is a **file-based bridge, not a continuous live MCP connection**; it never auto-imports or applies changes. Keep the configured directory private and accessible only to your user account.

### Checking a pending AI inbox proposal

In IDW AI STUDIO, click **SET INBOX** and select the existing local JSON file configured as `IDW_PROPOSAL_INBOX` for your MCP client. Click **CHECK INBOX** whenever you want to read it. The DAW validates the format and loads it into the editor, but does not apply it; use **REVIEW PROPOSAL** followed by **APPROVE & APPLY**. This is user-initiated checking, not background monitoring, and it does not automatically remove the inbox file.

When **CHECK INBOX** loads a valid pending proposal, the panel now displays its action type (tempo, track creation, or mix adjustment) before the mandatory review and approval steps.

Inbox import additionally checks the actual UTF-8 payload size after reading the file, protecting against a file being enlarged between the initial size check and the read. It never auto-executes an imported command.

### Timeline editing foundation

The session model now exposes validated `moveClip(trackIndex, clipIndex, newStartBeat)` and `resizeClip(trackIndex, clipIndex, newLengthBeats)` operations. Negative positions, nonpositive durations, nonfinite values, and missing clips are rejected without mutation. Persistence tests cover moving and resizing and saving the results. These are model operations for future timeline editing UI, **not yet drag handles or a complete MIDI piano roll**.

### Interactive audio clip timeline (experimental)

With transport stopped, drag the body of an existing audio clip to move it horizontally, or drag its rightmost 10 pixels to resize the duration. Positions and duration snap to quarter-beat increments. Edits are disabled during playback, recording or export. Dragging edits the in-memory session; use **SAVE PROJECT** to persist it. Overlaps sum by default. Use the clip context menu to apply a crossfade to a partially overlapping pair; see clip fades below.

### Selected clip keyboard editing

Click an audio clip on the main timeline to select it and focus the editor. Press **Left/Right Arrow** to nudge its start position by ¼ beat; hold **Shift** while pressing Left/Right to shorten or lengthen the clip by ¼ beat (minimum ¼ beat). This is disabled during playback, recording, and export. Keyboard shortcuts work when the main timeline has focus.

### MIDI piano-roll data foundation

MIDI tracks now store validated note events (MIDI pitch 0–127, velocity 1–127, start beat, positive duration). The session model supports adding, moving, transposing and resizing notes, and session JSON saves/loads the notes while continuing to accept older projects without a `notes` field. This is the **data/persistence foundation only**; there is not yet a visible piano-roll editor or scheduled MIDI note playback.

### Graphical MIDI piano roll (early prototype)

Select a MIDI track and click **PIANO ROLL** while playback/record/export is stopped. The initial piano-roll window draws a C3–C6 keyboard grid and the saved notes in the selected track; click the grid to add a one-beat note with velocity 100 snapped to quarter beats. Use **SAVE PROJECT** to persist changes. Moving/resizing individual piano-roll notes, scrollable pitches, MIDI playback, and virtual-instrument sound generation are **not yet implemented**. Piano-roll edits should not be made while the transport is running.

Piano-roll editor update: drag an existing MIDI note horizontally to change its start beat and vertically to transpose pitch. Drag near the note's right edge to resize its duration. Time adjustments snap to quarter beats. This is still an experimental editor without MIDI playback or instrument hosting.

MIDI note selection: click an existing note to highlight it gold and focus the piano-roll editor. Press Delete or Backspace to remove the selected note. Deleted notes are removed from the in-memory project; save the project to preserve the edit. Automated tests cover valid and invalid note deletion. MIDI playback is not yet available.

### MIDI playback scheduler foundation

`MidiScheduler.h` converts saved piano-roll notes to chronologically ordered MIDI note-on/note-off events and respects track mute/solo settings. Same-beat note-offs are ordered before note-ons. Tests cover timing and mute behavior. This is **event scheduling only**: the live audio callback does not yet emit MIDI and there is still no built-in synthesizer or plugin host to make the notes audible.

### Audible MIDI preview (experimental)

The built-in sine oscillator now renders piano-roll MIDI notes through the DAW audio callback during PLAY, including MIDI-only sessions. Notes follow pitch, duration, velocity and track gain/pan/mute/solo. This is a basic sound preview, **not yet VST3/AU hosting, MIDI device output, or a full polyphonic synth**. The prototype uses direct note rendering without allocation, and still needs latency, CPU-load and real hardware testing.

Synth renderer safety: MIDI note times are checked before conversion to absolute sample-frame indices so malformed or extremely long note data cannot overflow those conversions. The synth still requires hardware and performance testing.

### Synth presets (prototype)

The MIDI preview synth now has a **Keys / Bass / Lead** selector. Keys is a sine tone; Bass and Lead blend harmonics for different timbres. The preset is currently global to the audio preview and is not saved per track or per project. Automated tests check that the oscillators produce different sample output. Sound quality and live hardware operation are not yet certified.

### Per-track synth presets

Keys, Bass, and Lead are now saved separately on each MIDI track in V14 session JSON. Select a MIDI track with the track navigation controls, then choose its sound using the preset selector while stopped. Audio preview uses each MIDI track's chosen preset. Older session files default to Keys. Preset assignment is still a basic oscillator setting rather than VST/AU instrument support.

### MIDI synth envelope foundation

Each MIDI track now stores attack and release durations in milliseconds (0–2000 ms, defaults 5 ms). The oscillator shapes note onset and ending with these values, and sessions persist them with backward-compatible defaults. This is an **engine and project-model update**; on-screen attack/release controls have not yet been added. Automated tests cover accepted values, rejected values and save/load.

### On-screen synth envelope controls

Select a MIDI track and use the **ATTACK ms** and **RELEASE ms** sliders to set that track's envelope (0–2000 ms). The controls load the selected track's saved values and edits are refused during playback, recording and export. Save the session to persist changes. These sliders remain subject to hardware validation; playback-time editing is disabled.

Synth performance refinement: attack/release sample counts and preset selection are now computed once per MIDI note render rather than recalculated for every audio sample. Real-time CPU usage still requires profiling.

### Polyphonic preview protection

The built-in synth now applies conservative per-buffer voice-count normalization for overlapping MIDI notes, helping limit summed chord levels. A three-note chord regression test checks for finite, bounded output. This is a prototype safety measure, not a limiter or guaranteed clipping prevention; hardware and dense-arrangement testing remain necessary.

### Sample-rate-aware MIDI sound generation

The internal oscillator now skips MIDI fundamentals at or above half the current audio sample rate (Nyquist limit). This prevents invalid high-frequency fundamentals on low-rate devices, but the harmonic Bass/Lead sounds are not yet fully band-limited and can still alias. A regression test covers an extreme high MIDI note at 8 kHz sample rate.

### Harmonic filtering for Bass and Lead

The Bass and Lead preview oscillators now omit second/third harmonics whenever those partials meet or exceed the current audio device's Nyquist frequency. The remaining partial weights are adjusted to keep the sound audible. This prevents aliasing from these explicit overtones, but it is not a fully band-limited general synthesizer. Regression tests cover both low and normal sample rates.

Harmonic boundary refinement: comparisons now use the Nyquist frequency divided by harmonic index instead of multiplying fundamental frequencies; an added regression checks that a second harmonic exactly at Nyquist is excluded.

### MIDI pitch lookup optimization

The preview synthesizer now uses a fixed 128-note frequency lookup table rather than recalculating equal-tempered pitches for every note in each audio buffer. Existing render-continuity and pitch/audio tests remain part of the cross-platform CI suite. This optimization has not yet been CPU-benchmarked on hardware.

### Fourth MIDI instrument: Organ

Select **Organ** in a MIDI track's instrument dropdown to hear a two-harmonic organ-style oscillator. The second harmonic is omitted at or beyond the current sample-rate Nyquist limit. The Organ preset is saved with each track (preset ID 3); older sessions remain compatible. A regression test checks that Organ audio is audible and distinct from Keys, Bass, and Lead. This is a simple synthesized approximation, not a sampled organ.

### Fifth MIDI preset: Pad

A new **Pad** oscillator preset combines a dominant fundamental and quiet second harmonic, subject to the same sample-rate harmonic filtering. Choose Pad from the MIDI track instrument selector; per-track session persistence continues to support it (preset ID 4). This is an initial sustained pad-like tone, not a sampled synthesizer or full ADSR patch.

### Five-preset regression and playback editing guard

Automated persistence tests now round-trip all five Keys/Bass/Lead/Organ/Pad instrument selections through JSON. Starting playback hides the editable piano roll to reduce the risk of UI note changes racing with the audio renderer. This is not a complete thread-safe snapshot architecture; other project mutations during playback still require an explicit concurrency audit.

### Playback-time model mutation guard

Tempo changes and adding audio/MIDI tracks are now ignored during playback, recording, or export, preventing those controls from modifying audio-rendered session state midstream. Piano-roll editing also closes on Play. These narrow safeguards do not make the whole session model thread-safe; a complete cross-thread mutation audit is still required.

### Audio-operation guards: asynchronous file dialogs

The open-project file chooser now rechecks playback, recording and export state in its asynchronous callback before restoring a session. Audio import likewise checks all three states when its chooser completes. Track gain/pan edits are blocked during these audio operations. These safeguards reduce unsafe UI model mutations; they do not provide a complete immutable audio-thread session snapshot.

### Project save/export concurrency guards

Native session save, portable project export, and project restoration now check playback, recording, and export activity. The asynchronous save/export chooser callbacks also recheck these states at execution, preventing a file dialog opened while idle from saving a session after audio processing begins. This is a targeted safety guard rather than a full thread-safe audio snapshot system.

### Bounce, recording and take-import coordination

The stereo WAV bounce chooser now rejects an export if playback, recording, or another export starts before the asynchronous file selection completes. Recording actions are blocked during an active bounce/export, and importing a completed take checks these operations before mutating the track graph. These are UI-level guards; audio-device callback synchronization and detailed race testing remain future work.

### Transport operation guard

Pressing Play while playback, vocal recording or WAV export is active no longer restarts the transport and resets its timeline. Stop is ignored while a WAV export is in progress, so it cannot indirectly attempt to end and import an active recording during export. Normal playback and recording transport use is unchanged. These guards are not a substitute for full audio callback synchronization.

### Additional recording/track control restrictions

A new vocal recording cannot be started during ordinary playback; stopping an already active recording remains possible. MIDI instrument selection is disabled during playback, recording and export, and the internal audio-track creation helper rejects those active-operation states. These are further UI-level precautions, not a complete synchronization guarantee.

### Mixer controls after transport stop

When playback reaches the end of the arrangement or Stop is pressed, the mixer now refreshes its MIDI preset, Attack and Release control enabled states. This restores normal editing after the transport finishes. It is a user-interface state fix, not full audio-thread snapshot synchronization.

### Immutable playback session snapshot

At Play, V14 now makes an immutable copy of the current session and publishes it to the audio callback. The callback uses this copy for track mute/solo, audio clip arrangement, tempo, gain/pan and MIDI synthesis instead of reading the editable project directly. This reduces session-model read/write races. The snapshot is published and loaded using atomic shared-pointer operations; their real-time cost still needs profiling. Transport slots, device lifecycle and other shared fields still need a separate concurrency audit. Project edits while playing remain deliberately restricted.

Snapshot build portability: `DAW/Main.cpp` explicitly includes `<memory>` for `std::shared_ptr`, `std::make_shared`, and atomic shared-pointer free functions, avoiding reliance on transitive JUCE headers. Full platform compilation and audio callback validation are still required.

### Playback snapshot isolation regression

A new regression test copies a MIDI session, then edits pitch, timing and instrument assignment in the original and checks that the copied playback model is unchanged. The audio callback also documents its single-snapshot-per-block behavior. This test validates ordinary session copying, not simultaneous multithreaded operation or the real-time cost of atomic shared-pointer access.

### Device reconfiguration playback guard

When JUCE prepares the audio engine for a new audio device/sample rate/block size, V14 now marks the transport as not playing before it reconfigures playback buffers. This avoids inadvertently continuing a session across a changed device configuration. UI transport state and complete device lifecycle synchronization still need hands-on testing.

### Audio resource release guard

JUCE's `releaseResources()` callback now marks the transport stopped before releasing track transport resources. Together with the `prepareToPlay()` guard, this prevents the application's playback flag remaining active across the device lifecycle. Hardware device switching, timer/UI reset behavior and real-time synchronization still require testing.

### Device-suspension UI recovery

The message-thread timer now notices when audio playback was suspended by device preparation or resource release, restores the currently selected track's mixer control states, and stops the playback repaint timer when no recording or export is active. This complements device lifecycle transport guards; physical device switching remains untested.

### Suspended transport source cleanup

The message-thread playback recovery path now explicitly stops all track AudioTransportSources when playback is suspended (for example, following a device lifecycle reset), before refreshing mixer controls and stopping its timer. This closes a gap where the DAW transport flag was stopped but individual audio sources could remain running internally. Physical device change behavior still requires manual testing.

### Playback timer tempo consistency

The playback UI clock now uses the BPM captured in the immutable playback session snapshot, matching the tempo used in the audio callback. When no playback snapshot exists it falls back to the editable session's BPM. This avoids playhead drift if the live project tempo differs from the active snapshot; real-device timing tests remain outstanding.

### Snapshot-aligned arrangement end

The transport's end-of-arrangement calculation now reads note and clip ends from the same immutable playback session used by the audio renderer and playhead tempo. Edits to the live project cannot alter the active playback endpoint. The updated cross-platform CI build is required before treating this as verified.

### Playback clock input validation

The transport playhead calculation now checks that the audio sample rate and active snapshot tempo are finite and positive before calculating its beat position. Invalid values yield a zero playhead instead of an invalid number. This is defensive code; the newest cross-platform build and real-device timing behavior still need verification.

### Testable transport timing

`DAW/PlaybackTiming.h` provides standalone `playbackBeat(frame, sampleRate, bpm)` and `arrangementEndBeat(session)` functions used by the GUI transport. Persistence tests now check frame-to-beat conversion, invalid timing inputs and note-based arrangement end, reducing reliance on GUI-only timing validation. Real audio-device transport latency remains unmeasured.

### Mixed arrangement timing regression

Automated transport tests now verify the endpoint for mixed audio/MIDI sessions (the latest item wins), audio-only clips, and empty arrangements. These tests supplement the existing frame-to-beat and MIDI-only checks; they are not a substitute for sound-device latency tests.

### Long-session transport regressions

The transport test suite now includes a 32-track alternating audio/MIDI arrangement, checking that the latest musical event determines the song endpoint, and a 30-minute playback clock at 120 BPM. These are numerical correctness tests, not real-time CPU or audio hardware stress tests.

### Dense MIDI rendering regression

The automated tests now render 128 overlapping notes across 16 MIDI tracks using all five internal presets. They compare a 1,024-sample render against four 256-sample blocks and reject nonfinite or out-of-range output. This is a deterministic correctness stress case, not a CPU benchmark or proof of real-time performance.

### Offline MIDI render performance benchmark

When configured with `IDW_BUILD_DAW=ON` and `BUILD_TESTING=ON`, build target `IDWDAWMidiBenchmark` and run it manually to measure the time needed to render 256 stereo 256-frame blocks with 128 active notes on 16 MIDI tracks. It prints elapsed rendering milliseconds, the equivalent audio duration and their ratio. The target is registered as the `IDWDAWMidiBenchmarkSmoke` CTest test for numerical validity, without a real-time speed threshold; compare performance results on the same machine/configuration, preferably Release. Hardware audio callback latency and worst-case scheduling are still unmeasured.

### MIDI benchmark worst-block reporting

The offline MIDI benchmark now reports its slowest individual 256-frame render (`worst_block_ms`) beside the nominal audio callback time budget (`block_budget_ms`), as well as total throughput. Both stereo channels are checked for nonfinite values. Results include measurement overhead, are machine-dependent, and do not establish glitch-free real-time operation.

### Benchmark in automated test runs

`IDWDAWMidiBenchmarkSmoke` runs the offline 128-note render from CTest so CI can detect invalid audio output and benchmark crashes. It deliberately does **not** fail when rendering takes longer than the audio deadline: CI machine performance and scheduler contention vary, and this offline benchmark is not a real-time glitch/latency guarantee. Use `ctest -V -R IDWDAWMidiBenchmarkSmoke` to see timing output.

### Benchmark timing breakdown

The offline MIDI benchmark now prints `render_only_ms`, `avg_block_ms`, `best_block_ms`, and `worst_block_ms` in addition to total loop time and the audio deadline. Its per-block timings exclude buffer clears and sample validity checks, providing more useful before/after synth optimization comparisons. They still include clock instrumentation overhead and should not be interpreted as physical audio device latency.

### MIDI benchmark tail latency

The benchmark now prints `p95_block_ms` and `p99_block_ms`, computed from its 256 observed per-block rendering durations, alongside mean and worst-case time. This helps distinguish sporadic CI scheduling interruptions from consistently expensive blocks. Shared CI runner measurements are not controlled hardware audio latency measurements.

### Block deadline exceedance diagnostics

The benchmark also reports `over_budget_blocks` and `over_budget_pct` for measured 256-frame rendering blocks above the nominal 5.33 ms block interval at 48 kHz. An exceedance on a shared CI runner is a performance warning, not proof of a real hardware underrun; the count is diagnostic only and does not fail CTest.

### Benchmark warm-up

The offline MIDI benchmark now performs 32 untimed 256-frame synth render blocks before capturing 256 measured blocks. This reduces cold-cache and initialization effects on reported tail latency. Results remain dependent on shared CI scheduling and do not demonstrate real-time hardware performance.

### Additional audio buffer-size smoke checks

The MIDI benchmark now exercises 64, 128, and 512 sample stereo buffers in addition to its timed 256-sample benchmark. Each size renders several blocks and checks both channels for nonfinite values. These checks provide configuration coverage, not latency measurements for the additional buffer sizes.

### Buffer-size timing diagnostics

The offline MIDI benchmark now prints average and worst render time alongside each audio deadline for 64-, 128-, and 512-sample blocks, supplementing the detailed 256-sample timing results. Each alternate buffer-size measurement currently covers eight blocks and should be treated as a diagnostic sample, not statistically robust performance evidence or measured hardware latency.

### Comparable multi-buffer render samples

The 64-, 128-, and 512-frame benchmarks now warm up for 32 blocks and measure 256 blocks each, reporting mean, P95, P99, worst block time, and nominal-deadline exceedance count. These are offline CI timings; audio-interface driver overhead, scheduling and hardware dropout behavior are not measured.

### Low-buffer deadline overrun diagnostics

For 64-, 128-, and 512-frame benchmarks, `total_overrun_ms` now sums the milliseconds by which individual measured render calls exceed their nominal audio callback budget. This distinguishes a few small misses from large scheduling spikes. Measurements are offline on shared CI runners and do not establish hardware underrun counts.

### Worst individual deadline overrun

Low-buffer benchmark output now includes `worst_overrun_ms`, the largest amount by which any 64-, 128-, or 512-frame offline render exceeded its nominal deadline. This complements `total_overrun_ms` and helps distinguish occasional scheduling spikes from consistently slow processing. Shared CI results are diagnostic and are not measurements of physical driver dropouts.

### Corrected benchmark note duration

The benchmark's 128 MIDI notes now last 16 beats rather than 4 beats, ensuring that they remain sounding throughout the longest warmed-up 512-frame measurement (approximately 3.07 seconds at 48 kHz). Earlier 512-frame timings could include blocks after notes ended, making those numbers unsuitable for sustained-load comparisons. Re-establish performance baselines using the updated benchmark.

### Continuous benchmark timeline

The 256-frame benchmark now continues its absolute MIDI frame position after the 32-block warm-up rather than restarting at zero for timed measurement. This more faithfully exercises sustained note phase and envelope progression. It changes the workload timeline, so performance comparisons should use results from the updated version.

### Stereo clipping regression

The offline 128-note benchmark now rejects any nonfinite or out-of-range sample (absolute amplitude greater than 1.0) on either left or right output, across its 64-, 128-, 256-, and 512-frame tests. This detects sample-level clipping in the test renderer; it is not a substitute for audio-interface measurements or listening tests.

### First deadline miss diagnostics

The buffer-size benchmark reports `first_over_budget_block` (0-based, or -1 if no missed budget) alongside overrun duration metrics. This helps identify whether unusually slow renders cluster at the beginning or later in a sampled workload. Shared CI runner scheduling can still cause isolated long measurements.

### Missed block interval diagnostics

The low-buffer benchmark now reports `last_over_budget_block` in addition to the first missed block. Both use zero-based indices, or -1 when no block exceeds the nominal deadline. These fields help identify the span of observed scheduling spikes; they do not measure actual sound-card underruns.

### Cached pitch frequencies in voice counting

The MIDI renderer now uses its existing 128-entry equal-temperament pitch lookup table when deciding whether notes can sound below Nyquist during active-voice normalization. This avoids calling `pow` for every note during every audio block; the same table is also used for note rendering. Cross-platform build and regression validation is required for this optimization.

## V14 recording beta — headphone monitoring foundation

`IDWMultiTakeRecorder` now has an opt-in, gain-limited dry monitor path for the first armed input. Monitoring remains **OFF by default** and is reset to OFF when recording stops. The DAW UI now provides a **HEADPHONES ONLY** toggle during multi-input recording; it remains disabled during single-input recording. This requires device-specific testing. Never route the monitoring output to loudspeakers near an open microphone because acoustic feedback can result. Use closed-back headphones and a low output volume. This is not a guaranteed low-latency monitoring solution: driver buffer settings and interface hardware affect delay. Verify JUCE audio-callback mixing behavior on Windows and macOS before shipping.

## Recording beta — experimental overdubbing

Enable **PLAY BEAT WHILE RECORDING**, arm an **empty audio track**, then press **RECORD VOCAL**. The application starts playback of existing session audio/MIDI after the capture writer is started. Press **STOP RECORD** to stop playback, close the WAV and import the new take. The switch is off by default. Existing audio tracks are protected from overwriting when overdub is enabled.

**Limitations:** the recorded clip is anchored to beat zero for an overdub, but capture begins before playback and is not sample-aligned; an initial lead-in can remain in the take. Playback and capture are started sequentially, not at an identical sample boundary; reported input-driver latency trim is only an estimate. Do not treat this as sample-accurate synchronized overdubbing until loopback testing, device callback alignment and a calibrated round-trip latency offset have been implemented. Use headphones to avoid playback bleeding into the microphone.

### Manual take-alignment adjustment

The **TAKE TRIM ms** control adds a signed correction (-100 to +250 milliseconds) to the input device's reported latency. Zero preserves the old behavior. The adjusted trim is applied non-destructively as a clip source offset when each recorded WAV is imported; the WAV itself is preserved. Positive values remove more initial captured audio; negative values remove less (never less than zero). The value is captured when recording begins, not while recording. This is a manual troubleshooting aid, **not automatic loopback calibration or sample-accurate synchronization**. Test alignment with a transient/click recorded through a physical loopback before relying on the correction.

### Loopback timing regression (synthetic)

`DAW/LatencyAlignment.h` provides a deterministic pulse locator: supply a recorded float buffer, the expected sample index, and a search radius to obtain a signed sample lag and millisecond conversion. `DAW/SessionPersistenceTests.cpp` checks a pulse 96 samples late, 96 samples early, and absent input at 48 kHz. These are **synthetic unit tests**, not measurements of the user's audio interface. A physical loopback recording, matched clock reference, and safe repeatability tests are still needed before offering automatic latency calibration.

### Repeated-pulse calibration analysis

The timing helper now supports analyzing at least three known loopback pulse positions and rejecting inconsistent pulse delays. It requires at least 75% of expected pulses to match the measured median within eight samples (configurable). Synthetic tests cover four consistent pulses, one outlier, and silence rejection. This is an **offline analysis building block only**, not a working hardware loopback wizard. Nothing changes saved trim automatically; the existing manual TAKE TRIM control stays under user control.

### Offline loopback WAV verification

`DAW/LoopbackWavAnalysis.h` now loads a previously recorded WAV file and measures timing against known expected pulse indices on a selected input channel. The test suite generates a real 48 kHz PCM WAV with five delayed pulses and verifies the reported 72-sample (1.5 ms) lag, including invalid-channel rejection. This helper currently requires a known pulse schedule and is not yet connected to an in-app calibration wizard. It **does not** automatically change project latency settings. Only recordings up to ten minutes are accepted by the initial offline implementation to bound memory use; avoid opening untrusted large files. Hardware-generated synchronized playback/recording pulses and round-trip testing remain outstanding.

### Analyze loopback WAV in the DAW

Use **ANALYZE LOOPBACK WAV** while idle, enter the *known* pulse timestamps in milliseconds and a 1-based WAV channel, then choose an existing recorded WAV (currently limited to approximately 60 seconds at 48 kHz). The app reports sample lag, milliseconds and matched pulse count; it does **not** change TAKE TRIM. This analysis is only meaningful if the chosen WAV came from playback with precisely known pulse emission times on the same timeline. A real interface calibration wizard that generates, plays, records and verifies loopback impulses is not yet available. Never plug a powered speaker output directly into a microphone input; follow your interface's supported line-level routing.

### Reference pulse WAV generation

`DAW/LoopbackReference.h` can generate a mono 16-bit PCM WAV containing five or more known pulse positions (for example 100, 200, 300, 400, 500 ms at 48 kHz). Output pulse amplitude defaults to 0.25 (not full scale). The generator is currently a tested code helper, **not yet a UI control**. For an actual loopback, play the reference WAV through a supported interface line output routed to a line input, record the return in the DAW, and compare against the known pulse schedule; observe audio-interface electrical specifications. Do not connect amplified speaker outputs to an audio input. Automatic synchronized capture is still outstanding.

### Generate a calibration reference in the app

Click **CREATE PULSE WAV** while transport and recording are stopped, choose a new output filename, and save. The generated mono 48 kHz reference contains five known pulses at 100, 200, 300, 400, and 500 milliseconds. The generator does not overwrite existing WAV files. Play it externally through the interface's supported line-level output, record the returned signal on an input, then click **ANALYZE LOOPBACK WAV** and enter `100,200,300,400,500` to inspect the detected lag. Reference playback and capture are not automated or sample-clock synchronized yet; no correction is automatically applied. Use only electrically compatible line-level loopback connections.

## Recording beta hardware release gate

Before distributing a recording beta, complete the device-specific acceptance checklist in [`RECORDING_BETA_QA.md`](RECORDING_BETA_QA.md). CI compilation and synthetic loopback tests do not replace physical interface latency, headphone monitoring, overdub alignment and 30-minute stress testing on Windows and macOS.


### Clip fades and crossfades

While stopped, drag a clip's small top fade handles to set its fade-in or fade-out
duration continuously. Right-click a clip for quarter-beat edge presets, clear
fades, linear/equal-power curve selection, or a crossfade with an overlapping clip
on the same track. The menu names the partner by its one-based clip index.
Ctrl/Cmd+Z undoes the entire operation (both crossfade edges); Ctrl/Cmd+Shift+Z
or Ctrl/Cmd+Y redoes it. Save Project persists duration and curve metadata; source
audio is never rewritten. Older sessions load with zero fades.

Linear crossfades maintain unity gain for identical correlated audio. Equal-power
crossfades maintain the sum of squared gains for different/uncorrelated audio;
identical signals can rise by about 3 dB at the midpoint, so leave mix headroom.
Overlapping fade-in/out regions within one clip multiply. The last audible
fade-out sample is zero. A one-sample crossfade selects the incoming sample.
Playback and WAV export use the same sample-aligned envelope, including seeks,
source offsets and processing-block boundaries.

Crossfade creates editable edge fades over the current partial overlap. It does
not link clips: after moving/resizing either clip, reapply the crossfade for the
new overlap. Contained/coincident clips, a third clip in the transition, or an
outside fade intruding on the transition are rejected. Trim/shorten clamps fades
to the new duration; extend keeps their duration. Split preserves outer fades and
creates unfaded internal edges, and refuses a split inside an existing fade to
avoid changing its envelope. Edits are disabled during playback/recording/export.

Automated coverage: `IDWDAWClipFades` tests production envelope gain, block/seek
invariance, fractional beats, crossfade laws, edit validation, split/trim and the
undo/redo restore path. `IDWDAWPersistence` covers metadata round trips, malformed
fades and legacy defaults. `IDWDAWBounce` checks actual stereo WAV samples across
fades/crossfades, mixed source rates, offsets and track mix, and verifies unchanged
source bytes. These tests do not establish hardware listening quality or UI
interaction correctness. Windows manual acceptance: drag both handles, apply each
curve, undo/redo a pair, save/reopen, listen while seeking into an overlap, and
compare the exported WAV. Previous basic multitrack tests were user-reported as
passed; new fade-specific Windows testing remains separate.


### Shared clip sample clock and malformed timing regression

Playback windows, fade boundaries and WAV export now use the same checked beat-to-frame
conversion. This fixes a one-sample discrepancy at some fractional clip positions
(for example 7.5 * 137 / (60 * 44100) beats at 137 BPM / 44.1 kHz), where an unfaded
clip could lose its first audible sample. Tests sweep half-sample boundaries across
four rates and inspect an actual stereo WAV export. Invalid/unrepresentable timing
is rejected before integer conversion; invalid playback windows are silent, and
invalid exports return an error without retaining a partial WAV. Tests also cover
cursor overflow, invalid fades/tempo/mix and preservation of an existing output.
These automated cases do not replace hardware audio or UI interaction testing.


### Actual transport regression for reused audio files

The callback now shares its clip-reading function with `IDWDAWClipTransport`.
This test opens real stereo WAV files with JUCE's reader and transport, exercises
repeated clips after source EOF, same-file crossfades, source offsets and seeking,
and compares equal-rate playback samples against actual WAV export at several
block sizes. It also checks repeated playback with a 48 kHz source at 44.1 kHz.
The helper restarts an EOF-stopped reader before a new active read and uses integer
frame seeking instead of a seconds round-trip. Continuous resampler tails are not
flushed just because the underlying reader has prefetched through EOF.
No audio device is opened by these tests. Hardware listening/latency and GUI
interaction validation remain separate.

### Cached waveform previews (experimental)

Imported WAV/AIFF audio clips now display a reduced-resolution waveform inside the
timeline. The peak cache is generated asynchronously by JUCE AudioThumbnail, so
long recordings are not decoded on the audio callback or every repaint. Split
clips, left trims and recorded-take latency offsets display the corresponding
source interval; moving a clip leaves its source waveform unchanged. Reopened
projects rebuild the preview cache from available source files. Missing audio
still appears as a clip without a waveform. The cache is visual only: no
recordings or source WAV bytes are modified.

`IDWDAWWaveform` checks source-time mapping for moved, split, trimmed, extended,
and malformed clips on Windows/macOS CI. Rendering appearance, large-file
loading, thumbnail timing and resize behavior still need a Windows GUI check.
This milestone does not yet include horizontal timeline zoom, waveform sample
editing or sample-accurate mouse snapping.

### Fine keyboard timeline edits

With a clip selected and playback stopped, **Alt+Left/Right** moves it by
1/16 beat; **Alt+Shift+Left/Right** resizes its right edge by 1/16 beat.
**Alt+[** trims the left edge and **Alt+]** trims the right edge by 1/16 beat.
Without Alt, the existing shortcuts still use quarter-beat increments.
These are metadata-only edits with the existing undo/redo history; they are
not sample-level waveform edits. Mouse dragging still uses quarter-beat snapping.

### Horizontal arrangement zoom

Use **Ctrl+=** (or **Ctrl++**) to zoom in, **Ctrl+-** to zoom out,
and **Ctrl+0** to reset to 100%. The arrangement ruler displays the current
zoom level (50%, 75%, 100%, 150%, 200%). The same scale drives clip
drawing, fade handles, clip hit testing, mouse dragging and playhead placement,
so zoom does not alter source offsets or project audio. This is a view-only
feature. **Shift+mouse wheel** over the ruler or arrangement scrolls the timeline
horizontally; ordinary mouse wheel still scrolls track lanes vertically.
Scrolling is bounded by the last clip plus a four-beat margin, and never
changes playback or saved clip positions. The ruler, waveform previews, fade
handles, playhead, clip hit testing and dragging all use the same scroll offset.
The default mouse snap remains quarter-beat; use **G** to change the grid.

`IDWDAWZoom` checks pixel-to-beat round trips at every supported scale.
A Windows GUI check is still required for keyboard focus, small clips, fade
handles and rendering at the zoom extremes.

### Adjustable arrangement snap grid

With the arrangement focused and playback stopped, press **G** to cycle:
**1/4 beat (default) → 1/16 beat → 1 beat → Off → 1/4 beat**.
The current mode appears beside the zoom indicator in the ruler.
The grid applies to dragging clips to move them, dragging their right edges
to resize them, and clicking the ruler to set the playhead. Fade handles
remain continuous for smooth fades, and existing Alt fine-edit keyboard
shortcuts are unchanged. Off preserves the dragged beat position subject
to model validation (minimum clip duration 0.001 beat when resizing).
Undo/redo continues to record actual clip metadata changes; no source
WAV bytes are modified. The snap choice is an editor preference for the
current app session, not saved in the project file.

`IDWDAWSnap` tests each mode's quantization, cycling and invalid input.
The Windows GUI test should confirm that G changes the label and that
dragging clips at each grid setting behaves as expected, including with
horizontal scrolling and zoom.
