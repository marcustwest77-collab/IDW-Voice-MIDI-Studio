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

With transport stopped, drag the body of an existing audio clip to move it horizontally, or drag its rightmost 10 pixels to resize the duration. Positions and duration snap to quarter-beat increments. Edits are disabled during playback, recording or export. Dragging edits the in-memory session; use **SAVE PROJECT** to persist it. Clip overlap and crossfades are not yet managed by this interface.

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
