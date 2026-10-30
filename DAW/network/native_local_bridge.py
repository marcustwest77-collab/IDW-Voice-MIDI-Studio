"""Development adapter: verified TLS sink -> native authenticated loopback queue.

Use only with host-controlled receiver/bridge lifetimes and protected bootstrap.
This client does not verify the TCP receiver's process identity, authenticate a
remote TLS peer, grant consent, or execute a DAW command. No listener is created.
"""
from dataclasses import dataclass
import hmac
import math
import re
import socket
import threading
import time

from command_protocol import parse_command
from verified_event_producer import VerifiedCommandEvent

_HEX = re.compile(r"[0-9a-f]{64}\Z")
_PEER = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")


@dataclass(frozen=True, repr=False)
class NativeLocalBootstrap:
    port: int
    epoch: int
    _secret: str
    peer_sha256: str
    connection_id: str

    def __post_init__(self):
        if (type(self.port) is not int or not 1 <= self.port <= 65535 or
                type(self.epoch) is not int or not 1 <= self.epoch <= 2**64 - 1 or
                any(not isinstance(value, str) or not _HEX.fullmatch(value)
                    for value in (self._secret, self.peer_sha256, self.connection_id))):
            raise ValueError("invalid native bootstrap")

    def __repr__(self):
        return f"NativeLocalBootstrap(port={self.port}, epoch={self.epoch}, credentials=<redacted>)"

    @classmethod
    def decode(cls, data: bytes):
        if type(data) is not bytes or len(data) != 207 or data[:5] != b"IDWB\x01":
            raise ValueError("invalid native bootstrap")
        try:
            return cls(int.from_bytes(data[5:7], "big"), int.from_bytes(data[7:15], "big"),
                       data[15:79].decode("ascii"), data[79:143].decode("ascii"),
                       data[143:207].decode("ascii"))
        except (UnicodeError, ValueError):
            raise ValueError("invalid native bootstrap") from None


class NativeLocalCommandBridge:
    def __init__(self, bootstrap: NativeLocalBootstrap, *, expected_peer_id: str,
                 verified_peer_sha256: str, verified_connection_id: str, timeout=1.0):
        # Caller supplies these only after the TLS owner verifies its actual peer
        # and matches current host consent. Strings alone cannot verify TLS.
        if (not isinstance(bootstrap, NativeLocalBootstrap) or
                not isinstance(expected_peer_id, str) or not _PEER.fullmatch(expected_peer_id) or
                not isinstance(verified_peer_sha256, str) or not _HEX.fullmatch(verified_peer_sha256) or
                not isinstance(verified_connection_id, str) or not _HEX.fullmatch(verified_connection_id) or
                not hmac.compare_digest(bootstrap.peer_sha256, verified_peer_sha256) or
                not hmac.compare_digest(bootstrap.connection_id, verified_connection_id) or
                type(timeout) not in (float, int) or not 0 < timeout <= 5 or not math.isfinite(timeout)):
            raise ValueError("invalid native bridge binding")
        self._bootstrap = bootstrap
        self._peer = expected_peer_id
        self._timeout = timeout
        self._lock = threading.Lock()

    def close(self):
        with self._lock:
            self._bootstrap = None  # Python string storage is not a secure wipe.

    def deliver(self, event: VerifiedCommandEvent, *, deadline=None) -> bool:
        with self._lock:
            bootstrap = self._bootstrap
            if (bootstrap is None or not isinstance(event, VerifiedCommandEvent) or
                    event.peer_id != self._peer or type(event.epoch) is not int or
                    event.epoch != bootstrap.epoch or not isinstance(event.wire, str)):
                return False
            try:
                wire = event.wire.encode("ascii")
                command = parse_command(wire)
                canonical = (f"1|{bootstrap.epoch}|{command['sequence']}|{command['request_id']}|"
                             f"{command['action']}|{command['track_index']}").encode("ascii")
                if (command['session_epoch'] != bootstrap.epoch or wire != canonical or
                        command['action'] not in ("arm", "select") or command['track_index'] >= 128):
                    return False
                return self._exchange(bootstrap, wire, deadline)
            except (OSError, ValueError, UnicodeError, OverflowError):
                return False

    def revoke(self, *, deadline=None) -> bool:
        """Authenticate terminal native admission; retire credentials on every outcome.

        A receipt means the native termination was admitted, never a DAW action.
        Failure is observable and must not be treated as confirmed revocation.
        """
        with self._lock:
            bootstrap, self._bootstrap = self._bootstrap, None
            if bootstrap is None:
                return False
            try:
                wire = f"IDW_REVOKE|1|{bootstrap.epoch}".encode("ascii")
                return self._exchange(bootstrap, wire, deadline)
            except (OSError, ValueError, UnicodeError, OverflowError):
                return False

    def _exchange(self, bootstrap, wire, deadline):
        payload = bootstrap._secret.encode("ascii") + b"\n" + wire
        if len(payload) > 4096:
            return False
        local_deadline = time.monotonic() + self._timeout
        if deadline is not None:
            if type(deadline) not in (int, float) or not math.isfinite(deadline):
                return False
            local_deadline = min(local_deadline, deadline)

        def remaining():
            value = local_deadline - time.monotonic()
            if value <= 0:
                raise TimeoutError("native handoff deadline")
            return value

        with socket.create_connection(("127.0.0.1", bootstrap.port), timeout=remaining()) as connection:
            connection.settimeout(remaining())
            connection.sendall(len(payload).to_bytes(4, "big") + payload)
            connection.settimeout(remaining())
            connection.shutdown(socket.SHUT_WR)  # Native receiver requires one frame followed by FIN.
            connection.settimeout(remaining())
            return connection.recv(1) == b"1"  # Queued, never approval/execution.
