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
                              timeout=2.0, cancel=None, deadline=None):
    """Read exactly one bootstrap, verify binding, consume, then acknowledge.

    Returns False on rejection, cancellation, timeout or OS failure. The borrowed
    server handle must remain valid until duplicated and have query/SYNCHRONIZE
    rights. It is never closed here. No process is opened from a caller PID.
    consume must be bounded, transactional, and return True only after accepting
    this binding. If acknowledgment fails, the server rolls back its session;
    the caller must discard any captured bootstrap when this returns False.
    Callback exceptions propagate with no acknowledgment. No secure Python wipe.
    An optional absolute monotonic deadline caps, never extends, the local timeout.
    """
    if (not isinstance(name, str) or not _NAME.fullmatch(name) or
            type(expected_server_handle) is not int or not 0 < expected_server_handle < 2**(8 * ctypes.sizeof(ctypes.c_void_p)) or
            type(expected_epoch) is not int or not 1 <= expected_epoch <= 2**64 - 1 or
            any(not isinstance(s, str) or not _HEX.fullmatch(s)
                for s in (expected_peer_sha256, expected_connection_id)) or not callable(consume) or
            type(timeout) not in (int, float) or not .02 <= timeout <= 5 or not math.isfinite(timeout) or
            (deadline is not None and (type(deadline) not in (int, float) or
                not -1e12 <= deadline <= 1e12 or not math.isfinite(deadline))) or
            (cancel is not None and not isinstance(cancel, threading.Event))):
        raise ValueError("invalid private bootstrap configuration")
    if os.name != "nt":
        return False # No TCP, stdio, or insecure cross-platform fallback.
    cancel = cancel if cancel is not None else threading.Event()
    deadline = min(time.monotonic() + timeout, deadline) if deadline is not None else time.monotonic() + timeout
    if cancel.is_set() or time.monotonic() >= deadline:
        return False
    peer = wintypes.HANDLE()
    if not _duplicate(_current(), expected_server_handle, _current(), ctypes.byref(peer), 0, False, 2):
        return False
    pipe = None
    def active():
        return not cancel.is_set() and time.monotonic() < deadline and _wait(peer, 0) == 258
    try:
        pid = _pid(peer)
        if not pid or pid == os.getpid() or not active():
            return False
        while active():
            # Overlapped, non-inheritable; identification-only impersonation.
            pipe = _open(name, 0xC0000000, 0, None, 3, 0x40000000 | 0x00100000 | 0x00010000, None)
            if pipe != _INVALID:
                break
            pipe = None
            if ctypes.get_last_error() not in (2, 231):
                return False
            cancel.wait(min(.01, max(0, deadline - time.monotonic())))
        server_pid = wintypes.ULONG()
        if (not pipe or not active() or not _server_pid(pipe, ctypes.byref(server_pid)) or
                server_pid.value != pid):
            return False # Verify process before reading any credential bytes.

        def transfer(data, writing):
            offset = 0
            while offset < len(data) and active():
                event = _event(None, True, False, None)
                if not event:
                    return False
                operation = _Overlapped();operation.hEvent = event
                count = wintypes.DWORD()
                buffer = (ctypes.c_ubyte * (len(data) - offset)).from_buffer(data, offset)
                pending = False
                try:
                    done = (_write if writing else _read)(pipe, buffer, len(buffer),
                        ctypes.byref(count), ctypes.byref(operation))
                    if not done:
                        if ctypes.get_last_error() != 997:
                            return False
                        pending = True
                        while active():
                            status = _wait(event, 10)
                            if status == 0:
                                done = _result(pipe, ctypes.byref(operation), ctypes.byref(count), False)
                                pending = False
                                break
                            if status != 258:
                                break
                    if not done or not count.value:
                        return False
                    offset += count.value
                finally:
                    if pending:
                        _cancel_io(pipe, ctypes.byref(operation))
                        # Drain cancellation before releasing OVERLAPPED/buffer.
                        _result(pipe, ctypes.byref(operation), ctypes.byref(count), True)
                    _close(event)
            return offset == len(data) and active()

        data = bytearray(207)
        if not transfer(data, False):
            return False
        try:
            bootstrap = NativeLocalBootstrap.decode(bytes(data))
        except ValueError:
            return False
        if (bootstrap.epoch != expected_epoch or
                not hmac.compare_digest(bootstrap.peer_sha256, expected_peer_sha256) or
                not hmac.compare_digest(bootstrap.connection_id, expected_connection_id) or not active()):
            return False
        if consume(bootstrap) is not True or not active():
            return False
        return transfer(bytearray(_ACK), True)
    finally:
        if pipe and pipe != _INVALID:
            _close(pipe)
        _close(peer)
