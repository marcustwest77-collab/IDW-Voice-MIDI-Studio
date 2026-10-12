#!/usr/bin/env python3
"""IDW Studio MCP: local, read-only Model Context Protocol (stdio) server.

No API keys, network listener, project writes, or file-content disclosure.
Configure IDW_PROJECT_FILE to a saved IDW V14 .json project.
"""
import json
import os
import sys
from pathlib import Path

PROTOCOL = "2025-03-26"
TOOLS = [
    {"name": "idw_project_status", "description": "Read BPM, meter, and track count from the configured saved IDW project (read-only).",
     "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False}},
    {"name": "idw_list_tracks", "description": "List track names, types, and mix settings; never exposes source paths or audio content.",
     "inputSchema": {"type": "object", "properties": {}, "additionalProperties": False}},
    {"name": "idw_preview_tempo", "description": "Validate a proposed BPM without modifying the DAW or its project.",
     "inputSchema": {"type": "object", "properties": {"bpm": {"type": "number", "minimum": 20, "maximum": 300}},
                     "required": ["bpm"], "additionalProperties": False}},
    {"name": "idw_preview_track", "description": "Preview adding one audio or MIDI track without modifying the project.",
     "inputSchema": {"type": "object", "properties": {"type": {"type": "string", "enum": ["audio", "midi"]}},
                     "required": ["type"], "additionalProperties": False}}
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

def dispatch(name, args):
    if name == "idw_preview_tempo":
        bpm = args.get("bpm")
        if isinstance(bpm, bool) or not isinstance(bpm, (int, float)) or not (20 <= bpm <= 300):
            raise ValueError("BPM must be between 20 and 300.")
        return {"proposal": "set_tempo", "bpm": bpm, "applied": False, "note": "Confirm and set tempo manually in V14."}
    if name == "idw_preview_track":
        kind = args.get("type")
        if kind not in ("audio", "midi"):
            raise ValueError("Track type must be audio or midi.")
        return {"proposal": "add_track", "type": kind, "applied": False,
                "note": "Confirm and add the track manually in V14."}
    data = project()
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
