"""Explicit, single-use localhost mTLS receiver for the native review pathway.

No CLI, automatic listener, certificate discovery, or native provisioning policy.
The host owns the context, consent callbacks, epoch and native lease provider.
"""
import math
import select
import socket
import ssl
import threading
import time

from native_tls_session import run_native_tls_session


def _require_context(context):
    if (not isinstance(context, ssl.SSLContext) or
            context.protocol != ssl.PROTOCOL_TLS_SERVER or
            context.verify_mode != ssl.CERT_REQUIRED or
            context.minimum_version < ssl.TLSVersion.TLSv1_2):
        raise ValueError("mandatory mTLS server context with TLS 1.2 minimum required")


class NativeTLSLoopbackReceiver:
    """Bind an ephemeral IPv4 loopback port only when explicitly constructed.

    serve_once is single-use, including failures. close requests cancellation;
    the caller must join its serving thread before disposing host callbacks.
    The context must remain exclusively host-controlled throughout the lifetime.
    """
    def __init__(self, context, *, accept_timeout=5.0, handshake_timeout=2.0):
        _require_context(context)
        for value in (accept_timeout, handshake_timeout):
            if type(value) not in (int, float) or not .05 <= value <= 10 or not math.isfinite(value):
                raise ValueError("receiver deadline must be between .05 and 10 seconds")
        self._context = context
        self._accept_timeout = accept_timeout
        self._handshake_timeout = handshake_timeout
        self._cancel = threading.Event()
        self._lock = threading.Lock()
        self._state = "ready"
        self._listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
                self._listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            self._listener.bind(("127.0.0.1", 0))
            self._port = self._listener.getsockname()[1]
            self._listener.listen(1)
            self._listener.setblocking(False)
        except BaseException:
            self._listener.close()
            raise

    @property
    def port(self):
        return self._port

    def close(self):
        with self._lock:
            self._cancel.set()
            if self._state == "ready":
                self._state = "closed"
                self._listener.close()

    def serve_once(self, **session_options):
        if "cancel" in session_options:
            raise ValueError("receiver owns cancellation; call close")
        with self._lock:
            if self._state != "ready":
                raise RuntimeError("receiver is single-use")
            self._state = "serving"
        raw = secure = None
        try:
            _require_context(self._context)
            deadline = time.monotonic() + self._accept_timeout
            while True:
                if not self._wait(self._listener, False, deadline):
                    return 0
                try:
                    raw, address = self._listener.accept()
                    break
                except BlockingIOError:
                    continue
            # Stop admission immediately: no second connection or retry on failure.
            self._listener.close()
            if address[0] != "127.0.0.1":
                raise PermissionError("loopback client required")
            raw.setblocking(False)
            _require_context(self._context)
            secure = self._context.wrap_socket(raw, server_side=True, do_handshake_on_connect=False)
            raw = None # SSLSocket now owns the descriptor.
            deadline = time.monotonic() + self._handshake_timeout
            while True:
                if self._cancel.is_set():
                    return 0
                if time.monotonic() >= deadline:
                    raise TimeoutError("native TLS handshake deadline")
                try:
                    secure.do_handshake()
                    break
                except ssl.SSLWantReadError:
                    if not self._wait(secure, False, deadline):
                        return 0
                except ssl.SSLWantWriteError:
                    if not self._wait(secure, True, deadline):
                        return 0
            if self._cancel.is_set():
                return 0
            return run_native_tls_session(secure, cancel=self._cancel, **session_options)
        finally:
            if secure is not None:
                secure.close()
            if raw is not None:
                raw.close()
            self._listener.close()
            with self._lock:
                self._state = "closed"

    def _wait(self, connection, writing, deadline):
        while not self._cancel.is_set():
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("native TLS receiver deadline")
            readable, writable, exceptional = select.select(
                [] if writing else [connection], [connection] if writing else [],
                [connection], min(.05, remaining))
            if exceptional:
                raise ConnectionError("native TLS receiver unavailable")
            if readable or writable:
                return True
        return False

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
