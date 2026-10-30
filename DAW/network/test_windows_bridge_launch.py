"""Managed launch parsing and real Windows parent-handle acquisition."""
from contextlib import contextmanager
import os
from pathlib import Path
import subprocess
import sys
import threading
import unittest
from unittest.mock import patch

import windows_bridge_launch as launch_api
from windows_bridge_launch import ManagedBridgeLaunch, parse_managed_bridge_arguments, receive_managed_windows_lease
from native_tls_session import TLSConnectionIdentity

NAME = "\\\\.\\pipe\\IDW-LocalBootstrap-" + "a" * 64


class LaunchContractTests(unittest.TestCase):
    def test_only_fixed_canonical_non_secret_arguments_are_accepted(self):
        self.assertEqual(parse_managed_bridge_arguments(["--idw-native-offer", NAME, "123", "45"]),
                         ManagedBridgeLaunch(NAME, 123, 45))
        for arguments in ([], ["--idw-native-bootstrap", NAME, "123", "45"],
                ["--idw-native-offer", NAME, "123", "45", "credential"],
                ["--idw-native-offer", NAME, "00123", "45"],
                ["--idw-native-offer", NAME, "123", "0"],
                ["--idw-native-offer", NAME, "123", "18446744073709551616"],
                ["--idw-native-offer", "\\\\remote\\pipe\\x", "123", "45"],
                ["--idw-native-offer", NAME, "4294967296", "45"]):
            with self.assertRaises(ValueError): parse_managed_bridge_arguments(arguments)
        with self.assertRaises(ValueError): ManagedBridgeLaunch(NAME, True, 45)

    def test_session_epoch_mismatch_opens_no_process(self):
        identity = TLSConnectionIdentity("guest", 46, "a" * 64, "b" * 64, "TLSv1.3")
        with patch.object(launch_api, "hold_launch_parent") as hold:
            with self.assertRaises(ValueError):
                receive_managed_windows_lease(ManagedBridgeLaunch(NAME, 123, 45), identity,
                                             cancel=threading.Event(), deadline=123.0)
            hold.assert_not_called()

    def test_composition_borrows_handle_only_during_private_offer_exchange(self):
        active = []
        @contextmanager
        def hold(_):
            active.append(True)
            try: yield 456
            finally: active.pop()
        identity = TLSConnectionIdentity("guest", 45, "a" * 64, "b" * 64, "TLSv1.3")
        cancel = threading.Event();sentinel = object()
        def receive(name, handle, supplied_identity, **options):
            self.assertEqual(active, [True]);self.assertEqual((name, handle), (NAME, 456))
            self.assertIs(supplied_identity, identity)
            self.assertEqual(options, dict(send_offer=True, cancel=cancel, deadline=123.0))
            return sentinel
        with patch.object(launch_api, "hold_launch_parent", side_effect=hold), \
                patch.object(launch_api, "receive_windows_native_lease", side_effect=receive):
            self.assertIs(receive_managed_windows_lease(ManagedBridgeLaunch(NAME, 123, 45), identity,
                                                      cancel=cancel, deadline=123.0), sentinel)
        self.assertEqual(active, [])

    @unittest.skipIf(os.name == "nt", "unsupported-platform test")
    def test_unsupported_platform_does_not_acquire_parent(self):
        with self.assertRaises(OSError):
            with launch_api.hold_launch_parent(ManagedBridgeLaunch(NAME, 123, 45)):
                self.fail("unsupported platform must not yield a handle")


@unittest.skipUnless(os.name == "nt", "Windows process parent tests")
class ParentLifetimeTests(unittest.TestCase):
    def run_child(self, parent, mode):
        result = subprocess.run([sys.executable, str(Path(__file__).resolve()), "--launch-child", str(parent), mode],
            capture_output=True, timeout=5, close_fds=True, creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
        return result.stdout.strip()

    def test_actual_parent_is_held_and_handle_is_closed_at_scope_exit(self):
        self.assertEqual(self.run_child(os.getpid(), "normal"), b"PARENT_OK")

    def test_unrelated_living_process_argument_is_rejected(self):
        unrelated = subprocess.Popen([sys.executable, "-c", "import time;time.sleep(10)"],
            close_fds=True, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            self.assertEqual(self.run_child(unrelated.pid, "normal"), b"DENIED")
            self.assertIsNone(unrelated.poll())
        finally:
            unrelated.terminate();unrelated.wait(timeout=2)

    def test_recycled_or_ambiguous_creation_time_is_rejected(self):
        # Inject ordering failure; do not claim actual PID reuse was reproduced.
        self.assertEqual(self.run_child(os.getpid(), "bad-time"), b"DENIED")


def child():
    launch = ManagedBridgeLaunch(NAME, int(sys.argv[2]), 45)
    try:
        manager = patch.object(launch_api, "_creation_time", side_effect=[200, 100]) if sys.argv[3] == "bad-time" else None
        if manager: manager.start()
        try:
            with launch_api.hold_launch_parent(launch) as handle:
                if launch_api._pid(handle) != launch.parent_pid or launch_api._wait(handle, 0) != 258:
                    raise RuntimeError("held parent unavailable")
            if launch_api._wait(handle, 0) != 0xFFFFFFFF:
                raise RuntimeError("borrowed parent handle was not released")
        finally:
            if manager: manager.stop()
        print("PARENT_OK")
    except PermissionError:
        print("DENIED")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--launch-child": child()
    else: unittest.main()
