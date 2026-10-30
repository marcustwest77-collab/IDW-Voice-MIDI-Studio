"""Transactional protected bootstrap -> revocable native lease, Windows only.

No launch, discovery, TLS verification or default activation. Use inside a trusted
provide_native_lease callback after real TLS verification and local host consent.
"""
import math
import re
import threading
import time

from native_tls_session import TLSConnectionIdentity, native_socket_lease
from windows_native_bootstrap import receive_windows_bootstrap


class NativeLeaseRollbackError(ConnectionError):
    """Both provisioning and rollback failed; original exceptions stay inspectable."""
    def __init__(self, provisioning_error, rollback_error):
        super().__init__("native lease provisioning failed and rollback was not confirmed")
        self.provisioning_error = provisioning_error
        self.rollback_error = rollback_error


def receive_windows_native_lease(name, expected_server_handle, identity, *,
                                 timeout=2.0, revoke_timeout=1.0, cancel=None, deadline=None):
    """Acknowledge only after constructing a lease bound to the actual TLS owner.

    Caller supplies trusted pipe discovery, a held server handle valid through
    this call, and identity from its real TLS session. These data do not establish
    trust by themselves. On any failed delivery/cancellation, close a constructed
    lease before raising. Failed rollback is reported separately, never hidden.
    The returned lease belongs to the caller and must always be closed. If host
    cancellation races return, the session owner must check cancellation and close
    that returned lease (run_native_tls_session already does so).
    """
    if (not isinstance(identity, TLSConnectionIdentity) or
            type(identity.epoch) is not int or not 1 <= identity.epoch <= 2**64 - 1 or
            not isinstance(identity.peer_id, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", identity.peer_id) or
            any(not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value)
                for value in (identity.peer_sha256, identity.connection_id)) or
            identity.tls_version not in ("TLSv1.2", "TLSv1.3") or
            type(revoke_timeout) not in (int, float) or not 0 < revoke_timeout <= 5 or
            not math.isfinite(revoke_timeout) or
            (deadline is not None and (type(deadline) not in (int, float) or
                not -1e12 <= deadline <= 1e12 or not math.isfinite(deadline))) or
            (cancel is not None and not isinstance(cancel, threading.Event))):
        raise ValueError("invalid native lease identity or revocation configuration")
    cancel = cancel if cancel is not None else threading.Event()
    lease = None
    def consume(bootstrap):
        nonlocal lease
        if cancel.is_set():
            return False
        if deadline is not None and time.monotonic() >= deadline:
            return False
        lease = native_socket_lease(bootstrap, identity, timeout=revoke_timeout)
        return True
    try:
        if cancel.is_set():
            raise ConnectionAbortedError("native lease provisioning canceled")
        if deadline is not None and time.monotonic() >= deadline:
            raise TimeoutError("native lease provisioning deadline")
        accepted = receive_windows_bootstrap(name, expected_server_handle,
            expected_epoch=identity.epoch, expected_peer_sha256=identity.peer_sha256,
            expected_connection_id=identity.connection_id, consume=consume,
            timeout=timeout, cancel=cancel, deadline=deadline)
        if cancel.is_set():
            raise ConnectionAbortedError("native lease provisioning canceled")
        if deadline is not None and time.monotonic() >= deadline:
            raise TimeoutError("native lease provisioning deadline")
        if not accepted or lease is None:
            raise ConnectionError("native private bootstrap was not acknowledged")
        return lease
    except BaseException as error:
        if lease is not None:
            try:
                lease.close()
            except BaseException as rollback:
                raise NativeLeaseRollbackError(error, rollback) from error
        raise
