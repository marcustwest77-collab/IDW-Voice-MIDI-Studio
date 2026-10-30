"""Session cancellation/deadline propagation to actual Windows pipe discovery."""
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

from native_tls_session import TLSConnectionIdentity
from windows_native_lease import receive_windows_native_lease
from windows_native_bootstrap import receive_windows_bootstrap
from test_native_local_bridge import issue_certificate
from test_native_tls_session import SessionHarness

NAME = "\\\\.\\pipe\\IDW-LocalBootstrap-" + "e" * 64
IDENTITY = TLSConnectionIdentity("guest", 45, "a" * 64, "b" * 64, "TLSv1.3")


class DeadlineTests(unittest.TestCase):
    def test_expired_deadline_never_contacts_pipe(self):
        with patch("windows_native_lease.receive_windows_bootstrap") as receive:
            with self.assertRaises(TimeoutError):
                receive_windows_native_lease(NAME, 123, IDENTITY, deadline=time.monotonic() - 1)
            receive.assert_not_called()

    def test_invalid_deadlines_are_rejected(self):
        for value in (True, float("nan"), float("inf"), 10**1000):
            with self.assertRaises(ValueError):
                receive_windows_native_lease(NAME, 123, IDENTITY, deadline=value)
            with self.assertRaises(ValueError):
                receive_windows_bootstrap(NAME, 123, expected_epoch=45,
                    expected_peer_sha256="a" * 64, expected_connection_id="b" * 64,
                    consume=lambda _: True, deadline=value)


@unittest.skipUnless(os.name == "nt", "actual Windows pipe wait required")
class LiveProvisioningLifetimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.cert, cls.key = issue_certificate(cls.folder.name, "localhost")
        cls.guest_cert, cls.guest_key = issue_certificate(cls.folder.name, "guest")

    @classmethod
    def tearDownClass(cls): cls.folder.cleanup()

    def host(self):
        # A real held living process handle, not a fabricated PID/handle. No pipe
        # is created: test the real OS discovery wait before any credential read.
        process = subprocess.Popen([sys.executable, "-c", "import time;time.sleep(15)"],
            close_fds=True, creationflags=subprocess.CREATE_NO_WINDOW)
        def cleanup():
            if process.poll() is None: process.terminate()
            process.wait(timeout=2)
        self.addCleanup(cleanup)
        return process

    def harness(self, **options):
        return SessionHarness(self.cert, self.key, self.guest_cert, self.guest_key,
                              owned_receiver=True, **options)

    def test_receiver_close_interrupts_pending_native_bootstrap(self):
        process = self.host();entered = threading.Event();context = []
        def provide(identity, *, cancel, deadline):
            import windows_native_bootstrap as bootstrap
            context.append((identity, cancel, deadline))
            real_open = bootstrap._open
            def observe_open(*args):
                result = real_open(*args)
                entered.set() # Actual OS discovery was attempted, not just provider entry.
                return result
            with patch("windows_native_bootstrap._open", side_effect=observe_open):
                return receive_windows_native_lease(NAME, int(process._handle), identity,
                    timeout=5, cancel=cancel, deadline=deadline)
        with self.harness(provision_hook=provide) as harness:
            harness.connect();self.assertTrue(entered.wait(2))
            started = time.monotonic();harness.cancel_session();harness.join()
            self.assertLess(time.monotonic() - started, 1)
            self.assertTrue(context[0][1].is_set())
            self.assertIsNone(harness.fixture);self.assertEqual(harness.revocations, 0)
            self.assertTrue(any(isinstance(error, ConnectionAbortedError) for error in harness.errors))
            identity = context[0][0]
            self.assertFalse(harness.ledger.approve("guest", 45, identity.connection_id, b"1|45|1|late|arm|1"))

    def test_session_deadline_caps_longer_native_bootstrap_timeout(self):
        process = self.host();entered = threading.Event()
        def provide(identity, *, cancel, deadline):
            entered.set()
            return receive_windows_native_lease(NAME, int(process._handle), identity,
                timeout=5, cancel=cancel, deadline=deadline)
        with self.harness(provision_hook=provide, session_timeout=.25) as harness:
            harness.connect();self.assertTrue(entered.wait(2))
            started = time.monotonic();harness.join()
            self.assertLess(time.monotonic() - started, 1.5)
            self.assertIsNone(harness.fixture)
            self.assertTrue(any(isinstance(error, TimeoutError) for error in harness.errors))

    def test_expired_consent_callback_cannot_start_native_provisioning(self):
        # Trusted callbacks must remain bounded; this models a late bounded UI response.
        calls = []
        with self.harness(provision_hook=lambda *args, **kwargs: calls.append(True), session_timeout=.1) as harness:
            def delayed_consent(_):
                time.sleep(.15)
                return True
            # Replace only the trusted test host response before connecting.
            # SessionHarness exposes this hook for this test below.
            harness.consent_hook = delayed_consent
            harness.connect();harness.join()
            self.assertEqual(calls, []);self.assertIsNone(harness.fixture)
            self.assertTrue(any(isinstance(error, TimeoutError) for error in harness.errors))


if __name__ == "__main__": unittest.main()
