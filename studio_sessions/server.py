"""IDW two-person signalling prototype. No audio/video is stored or relayed here."""
import asyncio
import hashlib
import hmac
import json
import os
import re
import secrets
import time
from pathlib import Path
from dataclasses import dataclass, field

from aiohttp import WSMsgType, web

MAX_MESSAGE = 65536


def digest(value):
    return hashlib.sha256(value.encode("utf-8")).digest()


@dataclass
class Room:
    expires: float
    tokens: dict
    peers: dict = field(default_factory=dict)
    control_session: str = ""


class Rooms:
    def __init__(self, clock=time.monotonic, ttl=3600, capacity=100):
        self.clock, self.ttl, self.capacity = clock, ttl, capacity
        self.rooms = {}

    def create(self):
        # Empty expired rooms may be reclaimed immediately; active ones are closed by sweep.
        for key, room in list(self.rooms.items()):
            if room.expires <= self.clock() and not room.peers:
                del self.rooms[key]
        if len(self.rooms) >= self.capacity:
            raise ValueError("capacity")
        room_id = secrets.token_urlsafe(18)
        host, guest = secrets.token_urlsafe(32), secrets.token_urlsafe(32)
        self.rooms[room_id] = Room(self.clock() + self.ttl,
                                  {"host": digest(host), "guest": digest(guest)})
        return {"room": room_id, "host_ticket": host, "guest_ticket": guest,
                "expires_in_seconds": self.ttl}

    def claim(self, room_id, ticket, peer):
        room = self.rooms.get(room_id)
        if room is None or room.expires <= self.clock():
            raise ValueError("unavailable")
        role = next((r for r, token in room.tokens.items()
                     if hmac.compare_digest(token, digest(ticket))), None)
        if role is None:
            raise ValueError("unauthorized")
        if role in room.peers:
            raise ValueError("seat_in_use")
        room.control_session = secrets.token_urlsafe(18)
        room.peers[role] = peer
        return role

    def release(self, room_id, role, peer):
        room = self.rooms.get(room_id)
        if room is not None and room.peers.get(role) is peer:
            room.control_session = ""
            del room.peers[role]
            return True
        return False


def valid_control(payload):
    """Wire shape only. Receiver must enforce local consent and execution checks."""
    if not isinstance(payload, dict):
        return False
    kind = payload.get("type")
    fields = {
        "control_request": {"request"},
        "control_grant": {"request", "grant", "track", "ttl_seconds"},
        "control_revoke": {"grant"},
        "control_command": {"grant", "sequence", "action"},
        "control_result": {"grant", "sequence", "status"},
    }
    if not isinstance(kind, str) or kind not in fields:
        return False
    if set(payload) != {"type", "session"} | fields[kind]:
        return False
    for key in ("session", "request", "track"):
        if key in payload and (not isinstance(payload[key], str)
                or re.fullmatch(r"[A-Za-z0-9_-]{1,128}", payload[key]) is None):
            return False
    # Decimal strings preserve uint64 values across JavaScript and C++.
    for key in ("grant", "sequence"):
        if key in payload and (not isinstance(payload[key], str)
                or re.fullmatch(r"[1-9][0-9]{0,19}", payload[key]) is None
                or int(payload[key]) > 18446744073709551615):
            return False
    if kind == "control_grant":
        return type(payload["ttl_seconds"]) is int and 1 <= payload["ttl_seconds"] <= 900
    if kind == "control_command":
        return payload["action"] in ("arm", "record", "stop")
    if kind == "control_result":
        return payload["status"] in ("succeeded", "denied", "not_ready", "failed")
    return True


def valid_signal(payload):
    if not isinstance(payload, dict):
        return False
    if valid_control(payload):
        return True
    if payload == {"type": "ready"}:
        return True
    if payload.get("type") in ("offer", "answer"):
        return set(payload) == {"type", "sdp"} and isinstance(payload["sdp"], str) and 0 < len(payload["sdp"]) <= 60000
    if payload.get("type") == "candidate":
        candidate = payload.get("candidate")
        if set(payload) != {"type", "candidate"}:
            return False
        if candidate is None:
            return True
        if not isinstance(candidate, dict) or not set(candidate) <= {"candidate", "sdpMid", "sdpMLineIndex", "usernameFragment"}:
            return False
        return (isinstance(candidate.get("candidate"), str)
                and len(candidate["candidate"]) <= 4096
                and (candidate.get("sdpMid") is None or isinstance(candidate["sdpMid"], str))
                and (candidate.get("usernameFragment") is None or isinstance(candidate["usernameFragment"], str))
                and (candidate.get("sdpMLineIndex") is None or
                     type(candidate["sdpMLineIndex"]) is int and 0 <= candidate["sdpMLineIndex"] <= 65535))
    return False


def create_app(create_key, allowed_origins, rooms=None):
    if not isinstance(create_key, str) or len(create_key) < 32:
        raise ValueError("A room-creation service key of at least 32 characters is required")
    registry = rooms if rooms is not None else Rooms()
    app = web.Application(client_max_size=MAX_MESSAGE)

    async def new_room(request):
        supplied = request.headers.get("Authorization", "")
        if not hmac.compare_digest(digest(supplied), digest("Bearer " + create_key)):
            raise web.HTTPUnauthorized()
        try:
            result = registry.create()
        except ValueError:
            raise web.HTTPServiceUnavailable(text="Room capacity reached")
        return web.json_response(result, status=201, headers={"Cache-Control": "no-store"})

    async def connect(request):
        # Browser origins must be explicitly configured; ticket auth is still required.
        if request.headers.get("Origin") not in allowed_origins:
            raise web.HTTPForbidden()
        ws = web.WebSocketResponse(max_msg_size=MAX_MESSAGE, heartbeat=20)
        await ws.prepare(request)
        room_id, role = None, None
        try:
            message = await asyncio.wait_for(ws.receive(), 10)
            if message.type != WSMsgType.TEXT:
                return ws
            try:
                auth = json.loads(message.data)
                if not isinstance(auth, dict) or set(auth) != {"type", "room", "ticket"} or auth["type"] != "join":
                    raise ValueError("unauthorized")
                if not isinstance(auth["room"], str) or not isinstance(auth["ticket"], str) or len(auth["ticket"]) > 128:
                    raise ValueError("unauthorized")
                room_id = auth["room"]
                role = registry.claim(room_id, auth["ticket"], ws)
            except (ValueError, KeyError):
                await ws.close(code=1008, message=b"Room or seat unavailable")
                return ws
            room = registry.rooms[room_id]
            other_role = "guest" if role == "host" else "host"
            await ws.send_json({"type": "joined", "role": role, "peer_present": other_role in room.peers, "control_session": room.control_session})
            other = room.peers.get(other_role)
            if other is not None:
                await other.send_json({"type": "peer_joined", "role": role, "control_session": room.control_session})
            window_start, count = registry.clock(), 0
            async for message in ws:
                if message.type != WSMsgType.TEXT:
                    break
                now = registry.clock()
                if now >= room.expires:
                    await ws.close(code=1008, message=b"Room expired")
                    break
                if now - window_start >= 1:
                    window_start, count = now, 0
                count += 1
                if count > 100:
                    await ws.close(code=1008, message=b"Signal rate exceeded")
                    break
                try:
                    payload = json.loads(message.data)
                except ValueError:
                    payload = None
                if payload == {"type": "leave"}:
                    await ws.close()
                    break
                if not valid_signal(payload):
                    await ws.close(code=1008, message=b"Invalid signal")
                    break
                if payload["type"].startswith("control_") and payload["session"] != room.control_session:
                    await ws.close(code=1008, message=b"Stale control session")
                    break
                other = room.peers.get(other_role)
                if other is None:
                    await ws.send_json({"type": "peer_unavailable"})
                else:
                    # Identity is assigned by the server; clients cannot choose a recipient.
                    await other.send_json({**payload, "from": role})
        except (asyncio.TimeoutError, ConnectionError):
            pass
        finally:
            if role and registry.release(room_id, role, ws):
                room = registry.rooms.get(room_id)
                if room:
                    for peer in list(room.peers.values()):
                        if not peer.closed:
                            try:
                                await peer.send_json({"type": "peer_left", "role": role})
                            except ConnectionError:
                                pass
            await ws.close()
        return ws

    async def shutdown(app):
        for room in list(registry.rooms.values()):
            for ws in list(room.peers.values()):
                await ws.close(code=1001, message=b"Service shutting down")

    app.on_shutdown.append(shutdown)

    async def lifetime(app):
        async def sweep():
            while True:
                await asyncio.sleep(10)
                for room_id, room in list(registry.rooms.items()):
                    if room.expires <= registry.clock():
                        registry.rooms.pop(room_id, None)
                        for ws in list(room.peers.values()):
                            await ws.close(code=1008, message=b"Room expired")
        task = asyncio.create_task(sweep())
        yield
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass
        for room in list(registry.rooms.values()):
            for ws in list(room.peers.values()):
                await ws.close(code=1001, message=b"Service shutting down")
        registry.rooms.clear()

    app.cleanup_ctx.append(lifetime)
    async def client_file(request):
        filename = request.match_info.get("filename", "index.html")
        if filename not in {"index.html", "app.mjs", "session.mjs", "session.css"}:
            raise web.HTTPNotFound()
        response = web.FileResponse(Path(__file__).parent / "web" / filename)
        response.headers.update({
            "Cache-Control": "no-store",
            "Referrer-Policy": "no-referrer",
            "X-Content-Type-Options": "nosniff",
            "Content-Security-Policy": "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'; media-src 'self' blob:; frame-ancestors 'none'; base-uri 'none'; form-action 'none'",
            "Permissions-Policy": "camera=(self), microphone=(self), geolocation=()",
        })
        return response

    app.router.add_get("/", client_file)
    app.router.add_get("/{filename:index.html|app.mjs|session.mjs|session.css}", client_file)
    app.router.add_post("/v1/rooms", new_room)
    app.router.add_get("/v1/connect", connect)
    return app


if __name__ == "__main__":
    origins = set(filter(None, os.environ.get("IDW_STUDIO_ORIGINS", "").split(",")))
    if not origins:
        raise SystemExit("Set IDW_STUDIO_ORIGINS to the exact allowed client origins")
    app = create_app(os.environ.get("IDW_STUDIO_CREATE_KEY", ""), origins)
    # Prototype binds locally. Production needs HTTPS/WSS and an authenticated room broker.
    web.run_app(app, host="127.0.0.1", port=8787, access_log=None)
