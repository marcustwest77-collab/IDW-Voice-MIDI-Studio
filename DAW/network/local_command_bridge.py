"""Local-only development IPC sink for authenticated TLS command events.

This is a client that forwards events to an explicitly configured loopback
receiver. It does not authenticate the receiver or create a listener. The
receiver MUST separately authenticate the sender before trusting any event.
Do not expose to LAN, use as a production security boundary, or enable
track control based only on this transport.
"""
import json
import socket
from verified_event_producer import VerifiedCommandEvent


class LocalCommandBridge:
    def __init__(self, port: int, *, session_token: str, timeout: float = 2.0):
        if type(port) is not int or not 1024 <= port <= 65535:
            raise ValueError("valid loopback port required")
        if not 0 < timeout <= 5:
            raise ValueError("bounded timeout required")
        if not isinstance(session_token, str) or len(session_token) < 32:
            raise ValueError("session authentication token required")
        self._token = session_token
        self._port = port
        self._timeout = timeout

    def deliver(self, event: VerifiedCommandEvent) -> bool:
        if not isinstance(event, VerifiedCommandEvent):
            return False
        if not event.peer_id or event.epoch <= 0 or not event.wire:
            return False
        packet = json.dumps({"type": "command", "epoch": event.epoch,
                             "peer": event.peer_id, "wire": event.wire,
                             "token": self._token},
                            separators=(",", ":")).encode("utf-8")
        if len(packet) > 4096:
            return False
        try:
            with socket.create_connection(("127.0.0.1", self._port),
                                          timeout=self._timeout) as connection:
                connection.settimeout(self._timeout)
                connection.sendall(len(packet).to_bytes(4, "big") + packet)
                return connection.recv(1) == b"1"
        except (OSError, TimeoutError):
            return False
