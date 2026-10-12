#!/usr/bin/env python3
"""IDW Studio MCP: local, read-only Model Context Protocol (stdio) server.

No API keys, network listener, project writes, or file-content disclosure.
Configure IDW_PROJECT_FILE to a saved IDW V14 .json project.
"""
import json
import math
import os
import sys
from pathlib import Path

PROTOCOL = "2025-03-26"
TOOLS = [
    {"name": "idw_stage_proposal", "description": "Stage a validated non-destructive proposal in an explicitly configured local JSON inbox; never executes it in the DAW.",
     "inputSchema": {"type": "object", "properties": {
         "action": {"type": "string", "enum": ["set_tempo", "add_track", "set_track_mix"]},
         "bpm": {"type": "number", "minimum": 20, "maximum": 300},
         "track_type": {"type": "string", "enum": ["audio", "midi"]},
         "track_index": {"type": "integer", "minimum": 0, "maximum": 127},
         "gain": {"type": "number", "minimum": 0, "maximum": 2},
         "pan": {"type": "number", "minimum": -1, "maximum": 1}},
         "required": ["action"], "additionalProperties": False}},
    {"name": "idw_session_snapshot", "description": "Read an explicitly exported sanitized DAW status JSON snapshot; not a live stream.",
     "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False}},
    {"name": "idw_project_status", "description": "Read BPM, meter, and track count from the configured saved IDW project (read-only).",
     "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False}},
    {"name": "idw_list_tracks", "description": "List track names, types, and mix settings; never exposes source paths or audio content.",
     "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False}},
    {"name": "idw_preview_tempo", "description": "Validate a proposed BPM without modifying the DAW or its project.",
     "inputSchema": {"type": "object", "properties": {"bpm": {"type": "number", "minimum": 20, "maximum": 300}},
                     "required": ["bpm"], "additionalProperties": False}},
    {"name": "idw_preview_track", "description": "Preview adding one audio or MIDI track without modifying the project.",
     "inputSchema": {"type": "object", "properties": {"type": {"type": "string", "enum": ["audio", "midi"]}},
                     "required": ["type"], "additionalProperties": False}},
    {"name": "idw_analyze_project", "description": "Analyze saved session metadata for empty tracks, solo/mute conflicts, and clips extending beyond their timeline; never reads audio or shares file paths.",
     "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False}},
    {"name": "idw_preview_mix", "description": "Preview a track gain and pan change using a saved project; does not modify files.",
     "inputSchema": {"type": "object", "properties": {
         "track_index": {"type": "integer", "minimum": 0},
         "gain": {"type": "number", "minimum": 0, "maximum": 2},
         "pan": {"type": "number", "minimum": -1, "maximum": 1}},
         "required": ["track_index", "gain", "pan"], "additionalProperties": False}}
]

def project():
    configured = os.environ.get("IDW_PROJECT_FILE")
    if not configured:
        raise ValueError("Set IDW_PROJECT_FILE to a saved IDW V14 project JSON file.")
    path = Path(configured).expanduser()
    if not path.is_file() or path.stat().st_size > 4_000_000:
        raise ValueError("Project not found or exceeds 4 MB limit.")
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict) or data.get("format") != "idw-daw-session" or data.get("version") != 1:
        raise ValueError("Not a supported IDW V14 session.")
    if not isinstance(data.get("tracks"), list):
        raise ValueError("Project tracks malformed.")
    return data

def session_snapshot():
    configured = os.environ.get("IDW_STATUS_FILE")
    if not configured:
        raise ValueError("Set IDW_STATUS_FILE to an explicitly exported IDW status JSON file.")
    path = Path(configured).expanduser()
    if not path.is_file() or path.stat().st_size > 131072:
        raise ValueError("Status snapshot not found or exceeds 128 KB limit.")
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if (not isinstance(data, dict) or data.get("format") != "idw-ai-status-v1"
        or not isinstance(data.get("tracks"), list) or len(data["tracks"]) > 128):
        raise ValueError("Unsupported IDW status snapshot.")
    tracks = []
    for t in data["tracks"]:
        if not isinstance(t, dict):
            raise ValueError("Malformed track metadata.")
        tracks.append({k: t.get(k) for k in (
            "name", "type", "gain", "pan", "mute", "solo", "clipCount")})
    return {"bpm": data.get("bpm"), "numerator": data.get("numerator"),
            "denominator": data.get("denominator"),
            "playing": data.get("playing"), "recording": data.get("recording"),
            "tracks": tracks, "source": "explicitly-exported-status", "live": False}

def stage_proposal(args):
    action = args.get("action")
    if action == "set_tempo" and set(args) == {"action", "bpm"}:
        bpm = args["bpm"]
        if type(bpm) not in (int, float) or not math.isfinite(bpm) or not 20 <= bpm <= 300:
            raise ValueError("Invalid BPM.")
        payload = {"type": action, "bpm": bpm}
    elif action == "add_track" and set(args) == {"action", "track_type"}:
        if args["track_type"] not in ("audio", "midi"):
            raise ValueError("Invalid track type.")
        payload = {"type": action, "track_type": args["track_type"]}
    elif action == "set_track_mix" and set(args) == {"action", "track_index", "gain", "pan"}:
        index, gain, pan = args["track_index"], args["gain"], args["pan"]
        if (type(index) is not int or not 0 <= index < 128
            or any(type(x) not in (int, float) or not math.isfinite(x) for x in (gain, pan))
            or not 0 <= gain <= 2 or not -1 <= pan <= 1):
            raise ValueError("Invalid track mix.")
        payload = {"type": action, "track_index": index, "gain": gain, "pan": pan}
    else:
        raise ValueError("Unsupported action or extra parameters.")
    configured = os.environ.get("IDW_PROPOSAL_INBOX")
    if not configured:
        raise ValueError("Set IDW_PROPOSAL_INBOX to a dedicated local JSON file path.")
    path = Path(configured).expanduser()
    if not path.parent.is_dir() or path.suffix.lower() != ".json" or path.is_symlink():
        raise ValueError("Invalid proposal inbox location.")
    # Stage only: the DAW never watches or auto-executes this file.
    # Refuse overwrites so separate AI requests cannot silently replace one another.
    proposal = {"format": "idw-ai-proposal-v1", "action": payload}
    encoded = json.dumps(proposal, separators=(",", ":"), allow_nan=False).encode("utf-8")
    try:
        with path.open("xb") as out:
            out.write(encoded)
    except FileExistsError:
        raise ValueError("Proposal inbox already contains a pending file; review and remove it first.")
    return {"staged": True, "applied": False, "note": "Use IMPORT JSON in IDW AI STUDIO, review and approve manually."}

def dispatch(name, args):
    if name == "idw_stage_proposal":
        return stage_proposal(args)
    if name == "idw_session_snapshot":
        return session_snapshot()
    if name == "idw_preview_tempo":
        bpm = args.get("bpm")
        if isinstance(bpm, bool) or not isinstance(bpm, (int, float)) or not (20 <= bpm <= 300):
            raise ValueError("BPM must be between 20 and 300.")
        return {"proposal": "set_tempo", "bpm": bpm, "applied": False,
                "approval_json": {"format": "idw-ai-proposal-v1", "action": {"type": "set_tempo", "bpm": bpm}},
                "note": "Copy approval_json into IDW AI STUDIO, review and approve manually."}
    if name == "idw_preview_track":
        kind = args.get("type")
        if kind not in ("audio", "midi"):
            raise ValueError("Track type must be audio or midi.")
        return {"proposal": "add_track", "type": kind, "applied": False,
                "approval_json": {"format": "idw-ai-proposal-v1", "action": {"type": "add_track", "track_type": kind}},
                "note": "Copy approval_json into IDW AI STUDIO, review and approve manually."}
    if name == "idw_preview_mix":
        import math
        index, gain, pan = (args.get(k) for k in ("track_index", "gain", "pan"))
        if (type(index) is not int or index < 0 or
            any(type(x) not in (int, float) or not math.isfinite(x) for x in (gain, pan)) or
            not (0 <= gain <= 2 and -1 <= pan <= 1)):
            raise ValueError("Invalid track index, gain, or pan.")
        data = project()
        if index >= len(data["tracks"]):
            raise ValueError("Track index outside project.")
        return {"proposal": "set_track_mix", "track_index": index,
                "approval_json": {"format": "idw-ai-proposal-v1",
                                  "action": {"type": "set_track_mix", "track_index": index,
                                             "gain": gain, "pan": pan}},
                "track_name": str(data["tracks"][index].get("name", ""))[:128],
                "gain": gain, "pan": pan, "applied": False,
                "note": "Confirm and adjust track controls manually in V14."}
    data = project()
    if name == "idw_analyze_project":
        tracks = data["tracks"]
        if len(tracks) > 128:
            raise ValueError("Project track count exceeds the supported limit.")
        warnings = []
        solo_count = sum(t.get("solo") is True for t in tracks if isinstance(t, dict))
        for i, track in enumerate(tracks):
            if not isinstance(track, dict):
                raise ValueError("Malformed track metadata.")
            clips = track.get("clips", [])
            if not isinstance(clips, list):
                raise ValueError("Malformed clip list.")
            if track.get("type") == "audio" and not clips:
                warnings.append({"track_index": i, "code": "empty_audio_track"})
            if track.get("solo") is True and track.get("mute") is True:
                warnings.append({"track_index": i, "code": "solo_and_mute"})
            if solo_count and track.get("solo") is not True and track.get("type") == "audio":
                warnings.append({"track_index": i, "code": "silenced_by_solo"})
            for clip in clips:
                if not isinstance(clip, dict):
                    raise ValueError("Malformed clip metadata.")
                start, length = clip.get("startBeat"), clip.get("lengthBeats")
                import math
                if (type(start) not in (int, float) or type(length) not in (int, float)
                    or not math.isfinite(start) or not math.isfinite(length)
                    or start < 0 or length <= 0):
                    warnings.append({"track_index": i, "code": "invalid_clip_timing"})
        return {"track_count": len(tracks), "warnings": warnings,
                "source": "saved-project-snapshot", "live": False, "applied": False}
    if name == "idw_project_status":
        return {"bpm": data.get("bpm"), "numerator": data.get("numerator"),
                "denominator": data.get("denominator"), "track_count": len(data["tracks"]),
                "source": "saved-project-snapshot", "live": False}
    if name == "idw_list_tracks":
        return {"tracks": [{"index": i, "name": t.get("name"), "type": t.get("type"),
                            "mute": t.get("mute"), "solo": t.get("solo"),
                            "gain": t.get("gain"), "pan": t.get("pan")}
                           for i, t in enumerate(data["tracks"]) if isinstance(t, dict)],
                "source": "saved-project-snapshot", "live": False}
    raise ValueError("Unknown tool.")

def reply(message):
    sys.stdout.write(json.dumps(message, separators=(",", ":"), allow_nan=False) + "\n")
    sys.stdout.flush()

def handle(req):
    method = req.get("method")
    rid = req.get("id")
    if rid is None:
        return
    try:
        if method == "initialize":
            result = {"protocolVersion": PROTOCOL, "capabilities": {"tools": {"listChanged": False}},
                      "serverInfo": {"name": "idw-studio-mcp", "version": "0.1.0"}}
        elif method == "ping":
            result = {}
        elif method == "tools/list":
            result = {"tools": TOOLS}
        elif method == "tools/call":
            params = req.get("params") or {}
            name = params.get("name")
            args = params.get("arguments") or {}
            if not isinstance(args, dict) or not any(t["name"] == name for t in TOOLS):
                raise ValueError("Invalid tool call.")
            try:
                value = dispatch(name, args)
                result = {"content": [{"type": "text", "text": json.dumps(value)}]}
            except (ValueError, OSError, json.JSONDecodeError) as exc:
                result = {"content": [{"type": "text", "text": str(exc)}], "isError": True}
        else:
            reply({"jsonrpc": "2.0", "id": rid, "error": {"code": -32601, "message": "Method not found"}})
            return
        reply({"jsonrpc": "2.0", "id": rid, "result": result})
    except Exception:
        reply({"jsonrpc": "2.0", "id": rid, "error": {"code": -32603, "message": "Internal error"}})

def main():
    for line in sys.stdin:
        try:
            request = json.loads(line)
            if isinstance(request, dict):
                handle(request)
        except (ValueError, TypeError):
            continue

if __name__ == "__main__":
    main()
