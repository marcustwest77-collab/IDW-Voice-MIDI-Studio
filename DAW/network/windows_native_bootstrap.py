"""Windows private native bootstrap consumer; no listener, process launch or CLI.

Caller supplies a held, trusted owner-selected server process handle and binding
from the actual TLS owner. A pipe name or numeric PID alone is never authority.
"""
import ctypes
from ctypes import wintypes
import hmac
import math
import os
import re
import threading
import time

from native_local_bridge import NativeLocalBootstrap
from native_tls_offer import NativeTLSOffer
from native_bridge_ready import NativeBridgeReady

_NAME = re.compile(r"\\\\\.\\pipe\\IDW-LocalBootstrap-[0-9a-f]{64}\Z")
_HEX = re.compile(r"[0-9a-f]{64}\Z")
_ACK = b"IDW_BOOTSTRAP_OK\n"


if os.name == "nt":
    class _Overlapped(ctypes.Structure):
        _fields_ = [("Internal", ctypes.c_size_t), ("InternalHigh", ctypes.c_size_t),
                    ("Offset", wintypes.DWORD), ("OffsetHigh", wintypes.DWORD),
                    ("hEvent", wintypes.HANDLE)]

    _k = ctypes.WinDLL("kernel32", use_last_error=True)
    def _function(name, arguments, result):
        fn = getattr(_k, name);fn.argtypes = arguments;fn.restype = result;return fn
    _close = _function("CloseHandle", [wintypes.HANDLE], wintypes.BOOL)
    _current = _function("GetCurrentProcess", [], wintypes.HANDLE)
    _pid = _function("GetProcessId", [wintypes.HANDLE], wintypes.DWORD)
    _duplicate = _function("DuplicateHandle", [wintypes.HANDLE, wintypes.HANDLE, wintypes.HANDLE,
        ctypes.POINTER(wintypes.HANDLE), wintypes.DWORD, wintypes.BOOL, wintypes.DWORD], wintypes.BOOL)
    _wait = _function("WaitForSingleObject", [wintypes.HANDLE, wintypes.DWORD], wintypes.DWORD)
    _open = _function("CreateFileW", [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
        ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE], wintypes.HANDLE)
    _server_pid = _function("GetNamedPipeServerProcessId", [wintypes.HANDLE,
        ctypes.POINTER(wintypes.ULONG)], wintypes.BOOL)
    _event = _function("CreateEventW", [ctypes.c_void_p, wintypes.BOOL, wintypes.BOOL,
        wintypes.LPCWSTR], wintypes.HANDLE)
    _read = _function("ReadFile", [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD,
        ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(_Overlapped)], wintypes.BOOL)
    _write = _function("WriteFile", _read.argtypes, wintypes.BOOL)
    _result = _function("GetOverlappedResult", [wintypes.HANDLE, ctypes.POINTER(_Overlapped),
        ctypes.POINTER(wintypes.DWORD), wintypes.BOOL], wintypes.BOOL)
    _cancel_io = _function("CancelIoEx", [wintypes.HANDLE, ctypes.POINTER(_Overlapped)], wintypes.BOOL)
    _INVALID = ctypes.c_void_p(-1).value


def receive_windows_bootstrap(name, expected_server_handle, *, expected_epoch,
                              expected_peer_sha256, expected_connection_id, consume,
                              timeout=2.0, cancel=None, deadline=None, offer=None, _connection=None):
    """Read exactly one bootstrap, verify binding, consume, then acknowledge.

    Returns False on rejection, cancellation, timeout or OS failure. The borrowed
    server handle must remain valid until duplicated and have query/SYNCHRONIZE
    rights. It is never closed here. No process is opened from a caller PID.
    consume must be bounded, transactional, and return True only after accepting
    this binding. If acknowledgment fails, the server rolls back its session;
    the caller must discard any captured bootstrap when this returns False.
    Callback exceptions propagate with no acknowledgment. No secure Python wipe.
    An optional absolute monotonic deadline caps, never extends, the local timeout.
    The explicit TLS-offer profile permits up to 30 seconds for local consent;
    the legacy no-offer profile retains its five-second maximum.
    """
    if (not isinstance(name, str) or not _NAME.fullmatch(name) or
            type(expected_server_handle) is not int or not 0 < expected_server_handle < 2**(8 * ctypes.sizeof(ctypes.c_void_p)) or
            type(expected_epoch) is not int or not 1 <= expected_epoch <= 2**64 - 1 or
            any(not isinstance(s, str) or not _HEX.fullmatch(s)
                for s in (expected_peer_sha256, expected_connection_id)) or not callable(consume) or
            type(timeout) not in (int, float) or not .02 <= timeout <= (30 if offer is not None else 5) or not math.isfinite(timeout) or
            (deadline is not None and (type(deadline) not in (int, float) or
                not -1e12 <= deadline <= 1e12 or not math.isfinite(deadline))) or
            (cancel is not None and not isinstance(cancel, threading.Event))):
        raise ValueError("invalid private bootstrap configuration")
    offer_data = None
    if offer is not None:
        if (not isinstance(offer, NativeTLSOffer) or offer.epoch != expected_epoch or
                offer.peer_sha256 != expected_peer_sha256 or offer.connection_id != expected_connection_id):
            raise ValueError("native offer binding mismatch")
        offer_data = bytearray(offer.encode())
    if os.name != "nt":
        return False # No TCP, stdio, or insecure cross-platform fallback.
    if _connection is not None:
        if (not isinstance(_connection, _VerifiedPipe) or _connection.name != name or
                _connection.source_handle != expected_server_handle or offer is None or
                _connection.ready_epoch != expected_epoch):
            raise ValueError("prepared readiness connection scope mismatch")
        connection = _connection
        connection.restrict(timeout, cancel, deadline)
    else:
        try:
            connection = _VerifiedPipe(name, expected_server_handle, timeout, cancel, deadline)
        except BootstrapConnectionError:
            return False
    try:
        if connection.used: raise RuntimeError("private bootstrap connection is single-use")
        connection.used = True
        if offer_data is not None and not connection.transfer(offer_data, True): return False
        data = bytearray(207)
        if not connection.transfer(data, False): return False
        try: bootstrap = NativeLocalBootstrap.decode(bytes(data))
        except ValueError: return False
        if (bootstrap.epoch != expected_epoch or
                not hmac.compare_digest(bootstrap.peer_sha256, expected_peer_sha256) or
                not hmac.compare_digest(bootstrap.connection_id, expected_connection_id) or not connection.active()):
            return False
        if consume(bootstrap) is not True or not connection.active(): return False
        return connection.transfer(bytearray(_ACK), True)
    finally:
        connection.close()


class BootstrapConnectionError(ConnectionError):
    """The held server could not be verified or readiness could not be published."""


class _VerifiedPipe:
    """One caller owns I/O. Cancel and join it before closing from another scope."""
    def __init__(self, name, server_handle, timeout, cancel, deadline):
        if os.name != "nt": raise BootstrapConnectionError("Windows private pipe required")
        self.name = name;self.source_handle = server_handle
        self.cancel = cancel if cancel is not None else threading.Event()
        self.phase_cancel = None
        self.deadline = min(time.monotonic()+timeout, deadline) if deadline is not None else time.monotonic()+timeout
        self.peer = wintypes.HANDLE();self.pipe = None;self.ready_epoch = None;self.used = False
        try:
            if self.cancel.is_set() or time.monotonic() >= self.deadline:
                raise BootstrapConnectionError("private pipe connection canceled or expired")
            if not _duplicate(_current(), server_handle, _current(), ctypes.byref(self.peer), 0, False, 2):
                raise BootstrapConnectionError("held server handle unavailable")
            pid = _pid(self.peer)
            if not pid or pid == os.getpid() or not self.active():
                raise BootstrapConnectionError("held server is not active")
            while self.active():
                self.pipe = _open(name, 0xC0000000, 0, None, 3, 0x40000000 | 0x00100000 | 0x00010000, None)
                if self.pipe != _INVALID: break
                self.pipe = None
                if ctypes.get_last_error() not in (2, 231): break
                self.cancel.wait(min(.01, max(0, self.deadline-time.monotonic())))
            server_pid = wintypes.ULONG()
            if (not self.pipe or not self.active() or not _server_pid(self.pipe, ctypes.byref(server_pid)) or
                    server_pid.value != pid):
                raise BootstrapConnectionError("private pipe server was not verified")
        except BaseException:
            self.close();raise

    def restrict(self, timeout, cancel, deadline):
        self.deadline = min(self.deadline, time.monotonic()+timeout)
        if deadline is not None: self.deadline = min(self.deadline, deadline)
        self.phase_cancel = cancel

    def active(self):
        return (bool(self.peer.value) and not self.cancel.is_set() and
                (self.phase_cancel is None or not self.phase_cancel.is_set()) and
                time.monotonic() < self.deadline and _wait(self.peer, 0) == 258)

    def transfer(self, data, writing):
        offset = 0
        while offset < len(data) and self.active():
            event = _event(None, True, False, None)
            if not event: return False
            operation = _Overlapped();operation.hEvent = event
            count = wintypes.DWORD()
            buffer = (ctypes.c_ubyte * (len(data)-offset)).from_buffer(data, offset)
            pending = False
            try:
                done = (_write if writing else _read)(self.pipe, buffer, len(buffer), ctypes.byref(count), ctypes.byref(operation))
                if not done:
                    if ctypes.get_last_error() != 997: return False
                    pending = True
                    while self.active():
                        status = _wait(event, 10)
                        if status == 0:
                            done = _result(self.pipe, ctypes.byref(operation), ctypes.byref(count), False)
                            pending = False;break
                        if status != 258: break
                if not done or not count.value: return False
                offset += count.value
            finally:
                if pending:
                    _cancel_io(self.pipe, ctypes.byref(operation))
                    _result(self.pipe, ctypes.byref(operation), ctypes.byref(count), True)
                _close(event)
        return offset == len(data) and self.active()

    def close(self):
        self.used = True
        if self.pipe and self.pipe != _INVALID: _close(self.pipe)
        self.pipe = None
        if self.peer.value: _close(self.peer);self.peer.value = None


class WindowsReadyBootstrap:
    """Verify/publish readiness once, retain that pipe through the TLS offer.

    No TLS verification or native authority is supplied by this object. Its owner
    must hold the launch parent, cancel/join I/O before close, and always dispose
    this scope, including TLS/consent failures before receive is called.
    """
    def __init__(self, name, server_handle, ready, *, timeout=30.0, cancel=None, deadline=None):
        if (not isinstance(ready, NativeBridgeReady) or not isinstance(name, str) or not _NAME.fullmatch(name) or
                type(server_handle) is not int or not 0 < server_handle < 2**(8*ctypes.sizeof(ctypes.c_void_p)) or
                type(timeout) not in (int, float) or not .02 <= timeout <= 30 or not math.isfinite(timeout) or
                (cancel is not None and not isinstance(cancel, threading.Event)) or
                (deadline is not None and (type(deadline) not in (int, float) or
                 not -1e12 <= deadline <= 1e12 or not math.isfinite(deadline)))):
            raise ValueError("invalid private readiness configuration")
        self._connection = _VerifiedPipe(name, server_handle, timeout, cancel, deadline)
        try:
            if not self._connection.transfer(bytearray(ready.encode()), True):
                raise BootstrapConnectionError("private readiness was not published")
            self._connection.ready_epoch = ready.epoch
        except BaseException:
            self.close();raise

    def receive(self, name, expected_server_handle, **options):
        try:
            return receive_windows_bootstrap(name, expected_server_handle, **options, _connection=self._connection)
        finally:
            self.close()

    def close(self): self._connection.close()
    def __enter__(self): return self
    def __exit__(self, *_): self.close()
