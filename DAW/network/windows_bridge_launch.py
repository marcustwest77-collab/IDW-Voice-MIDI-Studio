"""Managed bridge launch contract and held-parent acquisition, Windows only.

No CLI activation or TLS listener. Parentage is a launch relationship, not an
approved host image policy. Use only inside the trusted packaged bridge workflow;
artifact trust and TLS configuration must come from independent trusted policy.
"""
from contextlib import contextmanager
from dataclasses import dataclass
import ctypes
from ctypes import wintypes
import os
import re

from windows_native_lease import receive_windows_native_lease
from native_tls_session import TLSConnectionIdentity


@dataclass(frozen=True)
class ManagedBridgeLaunch:
    pipe_name: str
    parent_pid: int
    epoch: int

    def __post_init__(self):
        if (not isinstance(self.pipe_name, str) or
                not re.fullmatch(r"\\\\\.\\pipe\\IDW-LocalBootstrap-[0-9a-f]{64}", self.pipe_name) or
                type(self.parent_pid) is not int or not 1 <= self.parent_pid <= 2**32 - 1 or
                type(self.epoch) is not int or not 1 <= self.epoch <= 2**64 - 1):
            raise ValueError("invalid managed bridge launch")


def parse_managed_bridge_arguments(arguments):
    """Parse the four fixed non-secret arguments emitted by startWithOffer."""
    if (type(arguments) not in (list, tuple) or len(arguments) != 4 or
            any(type(value) is not str for value in arguments) or arguments[0] != "--idw-native-offer"):
        raise ValueError("managed offer arguments required")
    for value in arguments[2:]:
        if not re.fullmatch(r"[1-9][0-9]{0,19}", value):
            raise ValueError("canonical launch identity required")
    return ManagedBridgeLaunch(arguments[1], int(arguments[2]), int(arguments[3]))


if os.name == "nt":
    class _ProcessEntry(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
            ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
            ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", wintypes.LONG),
            ("dwFlags", wintypes.DWORD), ("szExeFile", wintypes.WCHAR * 260)]

    _k = ctypes.WinDLL("kernel32", use_last_error=True)
    def _fn(name, arguments, result):
        fn = getattr(_k, name);fn.argtypes = arguments;fn.restype = result;return fn
    _snapshot = _fn("CreateToolhelp32Snapshot", [wintypes.DWORD, wintypes.DWORD], wintypes.HANDLE)
    _first = _fn("Process32FirstW", [wintypes.HANDLE, ctypes.POINTER(_ProcessEntry)], wintypes.BOOL)
    _next = _fn("Process32NextW", _first.argtypes, wintypes.BOOL)
    _open = _fn("OpenProcess", [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD], wintypes.HANDLE)
    _close = _fn("CloseHandle", [wintypes.HANDLE], wintypes.BOOL)
    _wait = _fn("WaitForSingleObject", [wintypes.HANDLE, wintypes.DWORD], wintypes.DWORD)
    _pid = _fn("GetProcessId", [wintypes.HANDLE], wintypes.DWORD)
    _current = _fn("GetCurrentProcess", [], wintypes.HANDLE)
    _times = _fn("GetProcessTimes", [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4, wintypes.BOOL)
    _INVALID = ctypes.c_void_p(-1).value

    def _creation_time(handle):
        creation, exit_time, kernel, user = (wintypes.FILETIME() for _ in range(4))
        if not _times(handle, ctypes.byref(creation), ctypes.byref(exit_time), ctypes.byref(kernel), ctypes.byref(user)):
            raise PermissionError("process creation time unavailable")
        return creation.dwLowDateTime | (creation.dwHighDateTime << 32)

    def _actual_parent_pid():
        snapshot = _snapshot(2, 0) # TH32CS_SNAPPROCESS; no handles are inherited.
        if not snapshot or snapshot == _INVALID:
            raise PermissionError("launch parent snapshot unavailable")
        try:
            entry = _ProcessEntry();entry.dwSize = ctypes.sizeof(entry)
            valid = _first(snapshot, ctypes.byref(entry))
            for _ in range(65536):
                if not valid: break
                if entry.th32ProcessID == os.getpid(): return entry.th32ParentProcessID
                valid = _next(snapshot, ctypes.byref(entry))
            raise PermissionError("launch parent unavailable")
        finally:
            _close(snapshot)


@contextmanager
def hold_launch_parent(launch):
    """Borrow a non-inheritable query/SYNCHRONIZE handle for this scope only.

    Reject unrelated PID arguments and recycled/ambiguous parent identity. The
    pipe consumer still verifies its actual server against the held handle.
    This does not establish publisher trust or prevent same-user process injection
    or privileged parent spoofing; those remain outside this launch guard.
    """
    if not isinstance(launch, ManagedBridgeLaunch):
        raise ValueError("validated launch required")
    if os.name != "nt":
        raise OSError("managed bridge parent acquisition is Windows-only")
    if launch.parent_pid == os.getpid() or _actual_parent_pid() != launch.parent_pid:
        raise PermissionError("managed bridge parent argument rejected")
    parent = _open(0x00100000 | 0x1000, False, launch.parent_pid) # SYNCHRONIZE | QUERY_LIMITED_INFORMATION
    if not parent:
        raise PermissionError("held launch parent unavailable")
    try:
        if (_pid(parent) != launch.parent_pid or _wait(parent, 0) != 258 or
                not 0 < _creation_time(parent) < _creation_time(_current())):
            raise PermissionError("launch parent lifetime rejected")
        yield int(parent)
    finally:
        _close(parent)


def receive_managed_windows_lease(launch, identity, *, cancel, deadline):
    """Compose verified launch scope, private session offer and revocable lease."""
    if (not isinstance(launch, ManagedBridgeLaunch) or not isinstance(identity, TLSConnectionIdentity) or
            type(identity.epoch) is not int or identity.epoch != launch.epoch):
        raise ValueError("managed launch/session epoch mismatch")
    with hold_launch_parent(launch) as parent:
        return receive_windows_native_lease(launch.pipe_name, parent, identity,
            send_offer=True, cancel=cancel, deadline=deadline)
