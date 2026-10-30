"""Transactional protected bootstrap -> revocable native lease, Windows only.

No launch, discovery, TLS verification or default activation. Use inside a trusted
provide_native_lease callback after real TLS verification and local host consent.
"""
import math
import re
import threading

from native_tls_session import TLSConnectionIdentity, native_socket_lease
from windows_native_bootstrap import receive_windows_bootstrap


class NativeLeaseRollbackError(ConnectionError):
    """Both provisioning and rollback failed; original exceptions stay inspectable."""
    def __init__(self, provisioning_error, rollback_error):
        super().__init__("native lease provisioning failed and rollback was not confirmed")
        self.provisioning_error = provisioning_error
        self.rollback_error = rollback_error


def receive_windows_native_lease(name, expected_server_handle, identity, *,
                                 timeout=2.0, revoke_timeout=1.0, cancel=None):
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
            (cancel is not None and not isinstance(cancel, threading.Event))):
        raise ValueError("invalid native lease identity or revocation configuration")
    cancel = cancel if cancel is not None else threading.Event()
    lease = None
    def consume(bootstrap):
        nonlocal lease
        if cancel.is_set():
            return False
        lease = native_socket_lease(bootstrap, identity, timeout=revoke_timeout)
        return True
    try:
        if cancel.is_set():
            raise ConnectionAbortedError("native lease provisioning canceled")
        accepted = receive_windows_bootstrap(name, expected_server_handle,
            expected_epoch=identity.epoch, expected_peer_sha256=identity.peer_sha256,
            expected_connection_id=identity.connection_id, consume=consume,
            timeout=timeout, cancel=cancel)
        if not accepted or lease is None:
            raise ConnectionError("native private bootstrap was not acknowledged")
        if cancel.is_set():
            raise ConnectionAbortedError("native lease provisioning canceled")
        return lease
    except BaseException as error:
        if lease is not None:
            try:
                lease.close()
            except BaseException as rollback:
                raise NativeLeaseRollbackError(error, rollback) from error
        raise
