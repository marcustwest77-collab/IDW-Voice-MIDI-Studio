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
                    "clips": [{"sourcePath": "/private/do-not-expose.wav"}]}]
    }), encoding="utf-8")
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
        {"jsonrpc": "2.0", "id": 9, "method": "tools/call", "params": {"name": "idw_preview_mix", "arguments": {"track_index": 0, "gain": 3.0, "pan": 0}}}
    ]
    proc = subprocess.run([sys.executable, str(server)], input="\n".join(map(json.dumps, commands))+"\n",
                          capture_output=True, text=True, timeout=8,
                          env={**os.environ, "IDW_PROJECT_FILE": str(path)})
    assert proc.returncode == 0, proc.stderr
    outputs = [json.loads(line) for line in proc.stdout.splitlines()]
    assert len(outputs) == 9
    assert len(outputs[1]["result"]["tools"]) == 5
    assert '"bpm": 124' in outputs[2]["result"]["content"][0]["text"]
    assert "/private/do-not-expose.wav" not in proc.stdout
    assert '"applied": false' in outputs[4]["result"]["content"][0]["text"]
    assert outputs[6]["result"].get("isError") is True
    assert '"applied": false' in outputs[7]["result"]["content"][0]["text"]
    assert outputs[8]["result"].get("isError") is True
    assert '"bpm": 124' in path.read_text(encoding="utf-8")
    print("IDW MCP protocol smoke tests: PASS")
