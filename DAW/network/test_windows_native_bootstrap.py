"""Actual C++ private pipe -> Python TLS bridge; no installed beta or GUI."""
import os
import subprocess
import sys
import threading
import time
import unittest
from unittest.mock import patch

from windows_native_bootstrap import receive_windows_bootstrap
from native_tls_offer import NativeTLSOffer
from test_native_local_bridge import NativeFixture, binding
import test_native_tls_receiver as receivers
import test_native_tls_session as sessions
from verified_event_producer import VerifiedCommandEvent

NAME = "\\\\.\\pipe\\IDW-LocalBootstrap-" + "a" * 64


def options():
    return dict(expected_epoch=45, expected_peer_sha256="a" * 64,
                expected_connection_id="b" * 64, consume=lambda _: True, timeout=.15)


class ConfigurationTests(unittest.TestCase):
    def test_only_tls_offer_profile_accepts_the_longer_canceled_wait(self):
        cancel = threading.Event();cancel.set()
        offer = NativeTLSOffer(45, "a" * 64, "b" * 64, "TLSv1.3")
        self.assertFalse(receive_windows_bootstrap(NAME, 1, **(options() | dict(timeout=30, offer=offer, cancel=cancel))))
        for changes in (dict(timeout=6), dict(timeout=31, offer=offer)):
            with self.assertRaises(ValueError):
                receive_windows_bootstrap(NAME, 1, **(options() | changes | dict(cancel=cancel)))

    def test_invalid_input_is_rejected(self):
        for name, handle, changes in (("\\\\remote\\pipe\\x", 1, {}), (NAME, 0, {}),
                (NAME, True, {}), (NAME, 1, dict(expected_epoch=True)),
                (NAME, 1, dict(expected_peer_sha256="A" * 64)),
                (NAME, 1, dict(timeout=10**1000)), (NAME, 1, dict(cancel=True))):
            with self.assertRaises(ValueError):
                receive_windows_bootstrap(name, handle, **(options() | changes))

    @unittest.skipIf(os.name == "nt", "unsupported-platform check")
    def test_unsupported_platform_has_no_fallback(self):
        self.assertFalse(receive_windows_bootstrap(NAME, 1, **options()))


@unittest.skipUnless(os.name == "nt" and os.environ.get("IDW_NATIVE_LOCAL_FIXTURE"),
                     "Windows native fixture required")
class PrivatePipeTests(unittest.TestCase):
    def test_private_bootstrap_to_native_review_and_revoke(self):
        fixture = NativeFixture("a" * 64, "b" * 64, private_bootstrap=True)
        self.addCleanup(fixture.close)
        bridge = binding(fixture.bootstrap);self.addCleanup(bridge.close)
        self.assertTrue(bridge.deliver(VerifiedCommandEvent(45, "1|45|1|private|arm|1", "guest")))
        self.assertEqual(fixture.pending(), 1)
        self.assertEqual(fixture.pending("REVOKE"), 0)

    def test_binding_mismatch_and_callback_rejection_never_acknowledge(self):
        for changes in (dict(expected_epoch=46), dict(expected_peer_sha256="0" * 64),
                        dict(expected_connection_id="0" * 64), dict(consume=lambda _: False)):
            called = []
            def receive(name, handle, **arguments):
                arguments["consume"] = lambda value: (called.append(value) or True)
                return receive_windows_bootstrap(name, handle, **(arguments | changes))
            with self.subTest(changes=tuple(changes)), self.assertRaises(RuntimeError):
                NativeFixture("a" * 64, "b" * 64, private_bootstrap=True, receive=receive)
            self.assertEqual(called, [])

    def test_wrong_server_handle_is_rejected_before_read(self):
        unrelated = subprocess.Popen([sys.executable, "-c", "import time;time.sleep(10)"],
            close_fds=True, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            def receive(name, _handle, **arguments):
                with patch("windows_native_bootstrap._read") as read:
                    accepted = receive_windows_bootstrap(name, int(unrelated._handle), **arguments)
                    read.assert_not_called()
                    return accepted
            with self.assertRaises(RuntimeError):
                NativeFixture("a" * 64, "b" * 64, private_bootstrap=True, receive=receive)
            self.assertIsNone(unrelated.poll()) # Borrowed handle was not closed/killed.
        finally:
            unrelated.terminate();unrelated.wait(timeout=2)

    def test_callback_exception_closes_pipe_without_acknowledgment(self):
        def receive(name, handle, **arguments):
            def fail(_): raise RuntimeError("test callback failure")
            return receive_windows_bootstrap(name, handle, **(arguments | dict(consume=fail)))
        with self.assertRaisesRegex(RuntimeError, "test callback failure"):
            NativeFixture("a" * 64, "b" * 64, private_bootstrap=True, receive=receive)

    def test_missing_pipe_deadline_and_cancellation(self):
        # A held living fixture process is authority, but this unrelated name has no endpoint.
        fixture = NativeFixture("a" * 64, "b" * 64)
        self.addCleanup(fixture.close)
        for canceled in (False, True):
            cancel = threading.Event()
            if canceled: cancel.set()
            started = time.monotonic()
            self.assertFalse(receive_windows_bootstrap(NAME, int(fixture.process._handle),
                                                       **(options() | dict(cancel=cancel))))
            self.assertLess(time.monotonic() - started, .75)
        self.assertEqual(fixture.pending(), 0) # Original borrowed process handle remains usable.

    def test_consumer_cannot_acknowledge_after_deadline_or_cancellation(self):
        for kind in ("deadline", "cancel"):
            cancel = threading.Event()
            called = []
            def receive(name, handle, **arguments):
                def consume(value):
                    called.append(value)
                    if kind == "deadline": time.sleep(.15)
                    else: cancel.set()
                    return True
                return receive_windows_bootstrap(name, handle,
                    **(arguments | dict(consume=consume, cancel=cancel, timeout=.1)))
            with self.subTest(kind=kind), self.assertRaises(RuntimeError):
                NativeFixture("a" * 64, "b" * 64, private_bootstrap=True, receive=receive)
            self.assertEqual(len(called), 1) # Captured data must be discarded on False.


@unittest.skipUnless(os.name == "nt", "Windows private pipe integration")
class PrivatePipeTLSIntegrationTests(receivers.OwnedReceiverIntegrationTests):
    def harness(self, **options):
        return sessions.SessionHarness(self.cert, self.key, self.guest_cert, self.guest_key,
                                       owned_receiver=True, private_bootstrap=True, **options)


if __name__ == "__main__": unittest.main()
