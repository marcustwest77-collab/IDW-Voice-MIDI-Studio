#!/usr/bin/env python3
"""Fast dependency-free protocol smoke test for the IDW V14 MCP server."""
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

server = Path(__file__).with_name("idw_mcp_server.py")
with tempfile.TemporaryDirectory() as folder:
    path = Path(folder) / "session.json"
    path.write_text(json.dumps({
        "format": "idw-daw-session", "version": 1, "bpm": 124,
        "numerator": 4, "denominator": 4,
        "tracks": [{"name": "Vocal", "type": "audio", "mute": False,
                    "solo": False, "gain": 1.0, "pan": 0.0,
                    "clips": [{"sourcePath": "/private/do-not-expose.wav", "startBeat": 0, "lengthBeats": 4}]}]
    }), encoding="utf-8")
    status_path = Path(folder) / "status.json"
    status_path.write_text(json.dumps({"format": "idw-ai-status-v1", "bpm": 133,
        "numerator": 4, "denominator": 4, "playing": False, "recording": False,
        "tracks": [{"name": "Vocal", "type": "audio", "gain": 0.8, "pan": 0,
                    "mute": False, "solo": False, "clipCount": 1,
                    "sourcePath": "/secret/not-for-ai.wav"}]}), encoding="utf-8")
    inbox_path = Path(folder) / "ai-inbox.json"
    commands = [
        {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}},
        {"jsonrpc": "2.0", "method": "notifications/initialized", "params": {}},
        {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}},
        {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "idw_project_status", "arguments": {}}},
        {"jsonrpc": "2.0", "id": 4, "method": "tools/call", "params": {"name": "idw_list_tracks", "arguments": {}}},
        {"jsonrpc": "2.0", "id": 5, "method": "tools/call", "params": {"name": "idw_preview_tempo", "arguments": {"bpm": 145}}},
        {"jsonrpc": "2.0", "id": 6, "method": "tools/call", "params": {"name": "idw_preview_track", "arguments": {"type": "midi"}}},
        {"jsonrpc": "2.0", "id": 7, "method": "tools/call", "params": {"name": "idw_preview_tempo", "arguments": {"bpm": 500}}},
        {"jsonrpc": "2.0", "id": 8, "method": "tools/call", "params": {"name": "idw_preview_mix", "arguments": {"track_index": 0, "gain": 0.8, "pan": -0.3}}},
        {"jsonrpc": "2.0", "id": 9, "method": "tools/call", "params": {"name": "idw_preview_mix", "arguments": {"track_index": 0, "gain": 3.0, "pan": 0}}},
        {"jsonrpc": "2.0", "id": 10, "method": "tools/call", "params": {"name": "idw_analyze_project", "arguments": {}}},
        {"jsonrpc": "2.0", "id": 11, "method": "tools/call", "params": {"name": "idw_session_snapshot", "arguments": {}}},
        {"jsonrpc": "2.0", "id": 12, "method": "tools/call", "params": {"name": "idw_stage_proposal", "arguments": {"action": "set_tempo", "bpm": 148}}},
        {"jsonrpc": "2.0", "id": 13, "method": "tools/call", "params": {"name": "idw_stage_proposal", "arguments": {"action": "add_track", "track_type": "audio"}}}
    ]
    proc = subprocess.run([sys.executable, str(server)], input="\n".join(map(json.dumps, commands))+"\n",
                          capture_output=True, text=True, timeout=8,
                          env={**os.environ, "IDW_PROJECT_FILE": str(path), "IDW_STATUS_FILE": str(status_path), "IDW_PROPOSAL_INBOX": str(inbox_path)})
    assert proc.returncode == 0, proc.stderr
    outputs = [json.loads(line) for line in proc.stdout.splitlines()]
    assert len(outputs) == 13
    assert len(outputs[1]["result"]["tools"]) == 8
    assert '"bpm": 124' in outputs[2]["result"]["content"][0]["text"]
    assert "/private/do-not-expose.wav" not in proc.stdout
    assert '"applied": false' in outputs[4]["result"]["content"][0]["text"]
    tempo_preview = json.loads(outputs[4]["result"]["content"][0]["text"])
    track_preview = json.loads(outputs[5]["result"]["content"][0]["text"])
    assert tempo_preview["approval_json"] == {
        "format": "idw-ai-proposal-v1", "action": {"type": "set_tempo", "bpm": 145}}
    assert track_preview["approval_json"] == {
        "format": "idw-ai-proposal-v1", "action": {"type": "add_track", "track_type": "midi"}}
    assert outputs[6]["result"].get("isError") is True
    assert '"applied": false' in outputs[7]["result"]["content"][0]["text"]
    mix_preview = json.loads(outputs[7]["result"]["content"][0]["text"])
    assert mix_preview["approval_json"] == {
        "format": "idw-ai-proposal-v1", "action": {
            "type": "set_track_mix", "track_index": 0, "gain": 0.8, "pan": -0.3}}
    assert outputs[8]["result"].get("isError") is True
    analysis = json.loads(outputs[9]["result"]["content"][0]["text"])
    assert analysis["track_count"] == 1 and analysis["warnings"] == []
    assert analysis["live"] is False and analysis["applied"] is False
    snapshot = json.loads(outputs[10]["result"]["content"][0]["text"])
    assert snapshot["bpm"] == 133 and snapshot["live"] is False
    assert "/secret/not-for-ai.wav" not in proc.stdout
    assert json.loads(outputs[11]["result"]["content"][0]["text"])["applied"] is False
    assert outputs[12]["result"]["isError"] is True
    assert json.loads(inbox_path.read_text()) == {"format": "idw-ai-proposal-v1", "action": {"type": "set_tempo", "bpm": 148}}
    assert '"bpm": 124' in path.read_text(encoding="utf-8")
    print("IDW MCP protocol smoke tests: PASS")
