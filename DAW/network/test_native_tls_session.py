"""Real localhost TLS lifetime -> native revocation; no GUI/two-machine claims."""
from dataclasses import replace
import hashlib
import importlib.util
import os
import shutil
import socket
import ssl
import tempfile
import threading
import time
import unittest

from host_approval_ledger import HostApprovalLedger
from loopback_transport import send_frame, receive_frame
from native_local_bridge import NativeLocalBootstrap
from native_tls_session import NativeTLSLease, run_native_tls_session
from test_native_local_bridge import NativeFixture, issue_certificate


class NativeLeaseTests(unittest.TestCase):
    def setUp(self):
        self.bootstrap = NativeLocalBootstrap(12345, 45, "1" * 64, "a" * 64, "b" * 64)

    def test_close_is_exactly_once(self):
        calls = []
        lease = NativeTLSLease(self.bootstrap, lambda: calls.append(True))
        lease.close(); lease.close()
        self.assertEqual(calls, [True])

    def test_failed_revocation_is_reported_without_duplicate_callback(self):
        calls = []
        def revoke():
            calls.append(True)
            raise RuntimeError("native revoke failed")
        lease = NativeTLSLease(self.bootstrap, revoke)
        with self.assertRaisesRegex(RuntimeError, "native revoke failed"):
            lease.close()
        with self.assertRaisesRegex(RuntimeError, "native revoke failed"):
            lease.close()
        self.assertEqual(calls, [True])
        with self.assertRaises(ValueError):
            NativeTLSLease(self.bootstrap, None)

    def test_plain_socket_cannot_claim_tls_identity(self):
        with socket.socket() as raw:
            with self.assertRaises(TypeError):
                run_native_tls_session(raw, expected_peer_id="guest", approved_client_sha256="a" * 64,
                    epoch=45, host_consent=lambda _: True, provide_native_lease=lambda _: None,
                    host_approval_ledger=HostApprovalLedger())

    def test_concurrent_close_waits_for_native_revocation(self):
        entered, release, finished = threading.Event(), threading.Event(), threading.Event()
        calls = []
        def revoke():
            calls.append(True)
            entered.set()
            if not release.wait(2): raise RuntimeError("test teardown barrier")
        lease = NativeTLSLease(self.bootstrap, revoke)
        first = threading.Thread(target=lease.close)
        second = threading.Thread(target=lambda: (lease.close(), finished.set()))
        first.start()
        self.assertTrue(entered.wait(1))
        second.start()
        waited = not finished.wait(.05)
        release.set()
        first.join(timeout=1); second.join(timeout=1)
        self.assertTrue(waited and finished.is_set())
        self.assertEqual(calls, [True])


class SessionHarness:
    def __init__(self, cert, key, guest_cert, guest_key, *, pin=None, allowed=True, mismatch=None,
                 verify_mode=ssl.CERT_REQUIRED, **limits):
        self.cert, self.guest_cert, self.guest_key = cert, guest_cert, guest_key
        self.fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(guest_cert.read_text())).hexdigest()
        self.pin = pin if pin is not None else self.fingerprint
        self.allowed, self.mismatch = allowed, mismatch
        self.ledger = HostApprovalLedger()
        self.cancel = threading.Event()
        self.native_ready = threading.Event()
        self.errors, self.results, self.identities = [], [], []
        self.fixture = None
        self.revocations = 0
        self.client_socket = None
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        self.listener.bind(("127.0.0.1", 0))
        self.port = self.listener.getsockname()[1]
        self.listener.listen(1)
        self.listener.settimeout(3)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(str(cert), str(key))
        context.load_verify_locations(cafile=str(guest_cert))
        context.verify_mode = verify_mode
        options = dict(command_timeout=2.0, session_timeout=6.0, max_commands=8)
        options.update(limits)

        def provide(identity):
            self.identities.append(identity)
            # Native admission happens only after the real TLS pin check and
            # trusted test consent callback, using this socket's fresh identity.
            self.fixture = NativeFixture(identity.peer_sha256, identity.connection_id)
            bootstrap = self.fixture.bootstrap
            if self.mismatch == "epoch": bootstrap = replace(bootstrap, epoch=46)
            if self.mismatch == "pin": bootstrap = replace(bootstrap, peer_sha256="0" * 64)
            if self.mismatch == "connection": bootstrap = replace(bootstrap, connection_id="f" * 64)
            def revoke():
                self.fixture.pending("REVOKE")
                self.revocations += 1
            self.native_ready.set()
            return NativeTLSLease(bootstrap, revoke)

        def serve():
            try:
                with self.listener:
                    raw, _ = self.listener.accept()
                    with raw:
                        raw.settimeout(2)
                        with context.wrap_socket(raw, server_side=True) as secure:
                            result = run_native_tls_session(secure, expected_peer_id="guest",
                                approved_client_sha256=self.pin, epoch=45,
                                host_consent=lambda _: self.allowed,
                                provide_native_lease=provide, host_approval_ledger=self.ledger,
                                cancel=self.cancel, **options)
                            self.results.append(result)
            except Exception as error:
                self.errors.append(error)
        self.worker = threading.Thread(target=serve, daemon=True)
        self.worker.start()

    def connect(self, credentials=True):
        context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cafile=str(self.cert))
        if credentials:
            context.load_cert_chain(str(self.guest_cert), str(self.guest_key))
        raw = socket.create_connection(("127.0.0.1", self.port), timeout=3)
        self.client_socket = context.wrap_socket(raw, server_hostname="localhost")
        self.client_socket.settimeout(3)
        return self.client_socket

    def command(self, sequence, request, action="arm"):
        self.ledger.approve("guest", 45, request)
        payload = f"1|45|{sequence}|{request}|{action}|1".encode("ascii")
        send_frame(self.client_socket, payload)
        return receive_frame(self.client_socket)

    def join(self):
        self.worker.join(timeout=5)
        if self.worker.is_alive():
            raise RuntimeError("TLS session fixture did not stop")

    def close(self):
        self.cancel.set()
        if self.client_socket is not None:
            self.client_socket.close()
        self.listener.close()
        self.worker.join(timeout=5)
        if self.fixture is not None:
            self.fixture.close()

    def __enter__(self): return self
    def __exit__(self, *_): self.close()


@unittest.skipUnless(os.environ.get("IDW_NATIVE_LOCAL_FIXTURE") and
    (shutil.which("openssl") or importlib.util.find_spec("cryptography")), "native fixture and certificate tool required")
class LiveNativeTLSTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.cert, cls.key = issue_certificate(cls.folder.name, "localhost")
        cls.guest_cert, cls.guest_key = issue_certificate(cls.folder.name, "guest")

    @classmethod
    def tearDownClass(cls): cls.folder.cleanup()

    def harness(self, **options):
        return SessionHarness(self.cert, self.key, self.guest_cert, self.guest_key, **options)

    def ready(self, harness):
        self.assertTrue(harness.native_ready.wait(5))
        self.assertIsNotNone(harness.fixture)

    def revoked(self, harness):
        harness.join()
        self.assertEqual(harness.revocations, 1)
        self.assertEqual(harness.fixture.pending(), 0)

    def test_live_identity_multiple_commands_and_eof_revokes(self):
        with self.harness() as harness:
            client = harness.connect()
            self.ready(harness)
            identity = harness.identities[0]
            self.assertEqual(identity.peer_sha256, harness.fingerprint)
            self.assertEqual(identity.connection_id, harness.fixture.bootstrap.connection_id)
            self.assertEqual(len(identity.connection_id), 64)
            self.assertEqual(harness.command(1, "first"), b"1|45|1|first|arm|1")
            self.assertEqual(harness.fixture.pending(), 1)
            self.assertEqual(harness.command(2, "second", "select"), b"1|45|2|second|select|1")
            self.assertEqual(harness.fixture.pending(), 2)
            client.shutdown(socket.SHUT_WR) # EOF after all prior TLS bytes, without a reset.
            self.revoked(harness)
            self.assertEqual(harness.errors, [])
            self.assertEqual(harness.results, [2])

    def test_reconnect_gets_new_connection_identity(self):
        identities = []
        for _ in range(2):
            with self.harness() as harness:
                client = harness.connect()
                self.ready(harness)
                identities.append(harness.identities[0].connection_id)
                client.close()
                self.revoked(harness)
        self.assertNotEqual(*identities)

    def test_idle_host_cancellation_revokes_without_another_command(self):
        with self.harness() as harness:
            harness.connect()
            self.ready(harness)
            started = time.monotonic()
            harness.cancel.set()
            self.revoked(harness)
            self.assertLess(time.monotonic() - started, 1)
            self.assertEqual(harness.errors, [])

    def test_bad_pin_missing_certificate_and_host_denial_create_no_native_lease(self):
        for options, credentials in ((dict(pin="0" * 64), True), ({}, False), (dict(allowed=False), True),
                                     (dict(verify_mode=ssl.CERT_OPTIONAL), True)):
            with self.subTest(options=options, credentials=credentials), self.harness(**options) as harness:
                try:
                    client = harness.connect(credentials=credentials)
                    client.close()
                except OSError:
                    pass
                harness.join()
                self.assertIsNone(harness.fixture)
                self.assertEqual(harness.revocations, 0)
                self.assertTrue(harness.errors)

    def test_replay_ends_session_and_clears_native_pending_review(self):
        with self.harness() as harness:
            harness.connect()
            self.ready(harness)
            self.assertEqual(harness.command(1, "first"), b"1|45|1|first|arm|1")
            self.assertEqual(harness.fixture.pending(), 1)
            with self.assertRaises(OSError):
                harness.command(1, "replay")
            self.revoked(harness)
            self.assertTrue(any(isinstance(error, PermissionError) for error in harness.errors))

    def test_consent_withdrawal_blocks_next_command_and_revokes(self):
        with self.harness() as harness:
            harness.connect()
            self.ready(harness)
            harness.command(1, "first")
            harness.allowed = False
            with self.assertRaises(OSError):
                harness.command(2, "withdrawn")
            self.revoked(harness)

    def test_bootstrap_binding_mismatches_are_revoked(self):
        for mismatch in ("epoch", "pin", "connection"):
            with self.subTest(mismatch=mismatch), self.harness(mismatch=mismatch) as harness:
                harness.connect()
                self.ready(harness)
                self.revoked(harness)
                self.assertTrue(harness.errors)

    def test_malformed_truncated_and_idle_deadline_revoke(self):
        for kind in ("oversize", "truncated", "idle"):
            with self.subTest(kind=kind), self.harness(command_timeout=.2) as harness:
                client = harness.connect()
                self.ready(harness)
                if kind == "oversize": client.sendall((4097).to_bytes(4, "big"))
                if kind == "truncated":
                    client.sendall((10).to_bytes(4, "big") + b"1")
                    client.shutdown(socket.SHUT_WR) # Preserve prior TLS bytes before FIN.
                self.revoked(harness)
                self.assertTrue(harness.errors)

    def test_partial_progress_does_not_extend_frame_deadline(self):
        with self.harness(command_timeout=.3) as harness:
            client = harness.connect()
            self.ready(harness)
            client.sendall(b"\x00")
            time.sleep(.15)
            client.sendall(b"\x00")
            self.revoked(harness)
            self.assertTrue(any(isinstance(error, TimeoutError) for error in harness.errors))

    def test_command_budget_revokes_after_queue_receipt(self):
        with self.harness(max_commands=1) as harness:
            harness.connect()
            self.ready(harness)
            self.assertEqual(harness.command(1, "only"), b"1|45|1|only|arm|1")
            self.revoked(harness)
            self.assertEqual(harness.results, [1])
            self.assertEqual(harness.errors, [])

    def test_session_deadline_expires_before_command_deadline(self):
        with self.harness(session_timeout=1.5, command_timeout=5) as harness:
            harness.connect()
            self.ready(harness)
            self.assertEqual(harness.command(1, "first"), b"1|45|1|first|arm|1")
            self.assertEqual(harness.fixture.pending(), 1)
            self.revoked(harness)
            self.assertTrue(any(isinstance(error, TimeoutError) for error in harness.errors))


if __name__ == "__main__": unittest.main()
