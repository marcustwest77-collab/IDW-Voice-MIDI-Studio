"""Development session owner for an already handshaken, trusted server TLS socket.

No listener/CLI/identity UI is installed. Caller owns trusted SSLContext/CA setup,
host consent, monotonic epoch allocation, and a transactional native lease provider.
"""
from dataclasses import dataclass
import hashlib
import hmac
import math
import re
import secrets
import select
import ssl
import threading
import time

from channel_bound_session import BoundSession
from command_protocol import parse_command
from native_local_bridge import NativeLocalBootstrap, NativeLocalCommandBridge
from replay_guard import ReplayGuard
from verified_event_producer import VerifiedEventProducer

_HEX = re.compile(r"[0-9a-f]{64}\Z")
_PEER = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")


@dataclass(frozen=True)
class TLSConnectionIdentity:
    peer_id: str
    epoch: int
    peer_sha256: str
    connection_id: str  # Local nonce minted after this socket's verified handshake.
    tls_version: str


class NativeTLSLease:
    """A bootstrap plus mandatory, bounded, identity-scoped native revocation.

    Provider must roll back internally if it raises before returning this lease.
    A returned lease is always closed, including bootstrap-binding rejection.
    """
    def __init__(self, bootstrap, revoke):
        if not isinstance(bootstrap, NativeLocalBootstrap) or not callable(revoke):
            raise ValueError("native bootstrap and revocation hook required")
        self.bootstrap = bootstrap
        self._revoke = revoke
        self._lock = threading.RLock()
        self._failure = None

    def close(self):
        with self._lock:
            callback, self._revoke = self._revoke, None
            if callback is None:
                if self._failure is not None:
                    raise self._failure
                return
            try:
                callback() # Concurrent closers wait for this bounded hook.
            except BaseException as error:
                self._failure = error
                raise # Never hide a failed native revocation from any closer.


class _Canceled(Exception):
    pass


def run_native_tls_session(secure, *, expected_peer_id, approved_client_sha256, epoch,
                           host_consent, provide_native_lease, host_approval_ledger,
                           cancel=None, max_commands=16, session_timeout=30.0, command_timeout=2.0):
    """Own this TLS socket until EOF/budget/error/cancellation, then revoke native.

    Only a real server-side SSLSocket with mandatory peer verification is accepted.
    Socket ownership transfers here. Native lease/consent hooks are trusted local
    callbacks, never remote fields. Their execution must be bounded by the owner.
    Returns the number of queue receipts, not host approvals or executed actions.
    """
    if not isinstance(secure, ssl.SSLSocket):
        raise TypeError("server TLS socket required")
    bridge = lease = session = None
    accepted = 0
    try:
        if (not isinstance(expected_peer_id, str) or not _PEER.fullmatch(expected_peer_id) or
                not isinstance(approved_client_sha256, str) or not _HEX.fullmatch(approved_client_sha256) or
                type(epoch) is not int or not 1 <= epoch <= 2**64 - 1 or
                not callable(host_consent) or not callable(provide_native_lease) or
                not callable(getattr(host_approval_ledger, "contains", None)) or
                not callable(getattr(host_approval_ledger, "consume", None)) or
                type(max_commands) is not int or not 1 <= max_commands <= 32 or
                any(type(value) not in (int, float) or not 0.05 <= value <= limit or not math.isfinite(value)
                    for value, limit in ((session_timeout, 60), (command_timeout, 5))) or
                (cancel is not None and not isinstance(cancel, threading.Event))):
            raise ValueError("invalid native TLS session configuration")
        cancel = cancel if cancel is not None else threading.Event()
        if (not secure.server_side or secure.context.verify_mode != ssl.CERT_REQUIRED or
                secure.fileno() < 0 or secure.version() not in ("TLSv1.2", "TLSv1.3")):
            raise PermissionError("completed mutually verified server TLS handshake required")
        def verify_peer():
            certificate = secure.getpeercert(binary_form=True)
            if (secure.context.verify_mode != ssl.CERT_REQUIRED or not certificate or
                    not hmac.compare_digest(hashlib.sha256(certificate).hexdigest(), approved_client_sha256)):
                raise PermissionError("TLS client certificate pin rejected")
        verify_peer()
        identity = TLSConnectionIdentity(expected_peer_id, epoch, approved_client_sha256,
                                         secrets.token_hex(32), secure.version())
        # This local identifier is not a TLS exporter or tls-unique binding. It
        # belongs to this held socket's lifetime and is never accepted from wire.
        deadline = time.monotonic() + session_timeout

        def check(until):
            if cancel.is_set():
                raise _Canceled()
            if time.monotonic() >= until:
                raise TimeoutError("native TLS session deadline")

        def wait_io(writing, until):
            while True:
                check(until)
                timeout = min(.05, max(0, until - time.monotonic()))
                readable, writable, exceptional = select.select(
                    [] if writing else [secure], [secure] if writing else [], [secure], timeout)
                if exceptional:
                    raise ConnectionError("TLS channel unavailable")
                if readable or writable:
                    return

        def read_exact(count, until, eof=False):
            result = bytearray()
            while len(result) < count:
                check(until)
                try:
                    chunk = secure.recv(count - len(result))
                except ssl.SSLWantReadError:
                    wait_io(False, until)
                    continue
                except ssl.SSLWantWriteError:
                    wait_io(True, until)
                    continue
                if not chunk:
                    if eof and not result:
                        return None
                    raise ConnectionError("truncated TLS command frame")
                result.extend(chunk)
            return bytes(result)

        def send_receipt(payload, until):
            frame = len(payload).to_bytes(4, "big") + payload
            sent = 0
            while sent < len(frame):
                check(until)
                try:
                    count = secure.send(frame[sent:])
                except ssl.SSLWantReadError:
                    wait_io(False, until)
                    continue
                except ssl.SSLWantWriteError:
                    wait_io(True, until)
                    continue
                if count <= 0:
                    raise ConnectionError("TLS queue receipt unavailable")
                sent += count

        check(deadline)
        if host_consent(identity) is not True:
            raise PermissionError("host consent denied")
        lease = provide_native_lease(identity)
        if not isinstance(lease, NativeTLSLease):
            raise ValueError("native lease required")
        # Epoch must also match, not only the fingerprint/connection nonce.
        if lease.bootstrap.epoch != identity.epoch:
            raise PermissionError("native lease epoch mismatch")
        bridge = NativeLocalCommandBridge(lease.bootstrap, expected_peer_id=identity.peer_id,
            verified_peer_sha256=identity.peer_sha256, verified_connection_id=identity.connection_id,
            timeout=min(1.0, command_timeout))
        check(deadline)
        if host_consent(identity) is not True:
            raise PermissionError("host consent revoked during native setup")
        session = BoundSession(identity.peer_id, identity.epoch, identity.connection_id)
        if not session.admit_bound(identity.peer_id, identity.connection_id, True, True) or not session.grant(control=True):
            raise PermissionError("native TLS admission rejected")
        guard = ReplayGuard()
        until = deadline
        producer = VerifiedEventProducer(identity.peer_id, identity.epoch,
            lambda event: bridge.deliver(event, deadline=until))
        secure.setblocking(False)
        for _ in range(max_commands):
            check(deadline)
            until = min(deadline, time.monotonic() + command_timeout)
            header = read_exact(4, until, eof=True)
            if header is None:
                break
            size = int.from_bytes(header, "big")
            if not 1 <= size <= 4096:
                raise ValueError("invalid TLS command size")
            payload = read_exact(size, until)
            check(until)
            verify_peer()  # Revalidate after TLS I/O, including any renegotiation.
            if host_consent(identity) is not True:
                raise PermissionError("host consent revoked")
            command = parse_command(payload)
            if command["action"] not in ("arm", "select"):
                raise PermissionError("TLS action not enabled")
            if not host_approval_ledger.contains(identity.peer_id, identity.epoch, command["request_id"]):
                raise PermissionError("command lacks host approval")
            guard.inspect(session, payload, identity.peer_id, identity.connection_id)
            if not host_approval_ledger.consume(identity.peer_id, identity.epoch, command["request_id"]):
                raise PermissionError("single-use host approval unavailable")
            check(until)
            producer.accepted_command_sink(command, identity.peer_id)
            check(until)
            send_receipt(payload, until)
            accepted += 1
        return accepted
    except _Canceled:
        return accepted
    finally:
        if session is not None:
            session.revoke()
        try:
            if bridge is not None:
                bridge.close()
        finally:
            try:
                if isinstance(lease, NativeTLSLease):
                    lease.close()
            finally:
                secure.close()
