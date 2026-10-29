"""Development-only adapter from validated mTLS command sink to DAW event data.

Use exclusively as accepted_command_sink in tls_loopback_server. The server must
complete certificate pinning, peer mapping, host approval and replay checks first.
This module does not itself authenticate connections or expose an IPC endpoint.
"""
from dataclasses import dataclass
from typing import Callable


@dataclass(frozen=True)
class VerifiedCommandEvent:
    epoch: int
    wire: str
    peer_id: str


class VerifiedEventProducer:
    def __init__(self, expected_peer_id: str, epoch: int,
                 deliver: Callable[[VerifiedCommandEvent], bool]):
        if not expected_peer_id or epoch <= 0 or not callable(deliver):
            raise ValueError("expected peer, positive epoch and delivery callback required")
        self._peer_id = expected_peer_id
        self._epoch = epoch
        self._deliver = deliver

    def accepted_command_sink(self, cmd: dict, certificate_peer: str) -> None:
        """Called only by the already-authenticated and authorized TLS sink."""
        if certificate_peer != self._peer_id:
            raise PermissionError("authenticated peer mismatch")
        if cmd.get("session_epoch") != self._epoch:
            raise PermissionError("trusted epoch mismatch")
        action = cmd.get("action")
        if action not in ("select", "arm"):
            raise PermissionError("remote action forbidden")
        sequence = cmd.get("sequence")
        request_id = cmd.get("request_id")
        track = cmd.get("track_index")
        if (type(sequence) is not int or sequence <= 0 or
                not isinstance(request_id, str) or not request_id or
                "|" in request_id or "\n" in request_id or
                type(track) is not int or not 0 <= track < 128):
            raise PermissionError("invalid command fields")
        wire = f"1|{self._epoch}|{sequence}|{request_id}|{action}|{track}"
        if len(wire) > 4096:
            raise PermissionError("oversize command")
        if self._deliver(VerifiedCommandEvent(self._epoch, wire, certificate_peer)) is not True:
            raise PermissionError("DAW event handoff unavailable")
