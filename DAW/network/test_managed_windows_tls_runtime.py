"""Native test host -> real Python child TLS runtime -> private native review."""
import hashlib
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import threading
import unittest
from unittest.mock import patch
from contextlib import contextmanager

from managed_windows_tls_runtime import serve_managed_windows_tls_once
from native_command_approval import NativeCommandApprovalLedger
from windows_bridge_launch import ManagedBridgeLaunch, parse_managed_bridge_arguments
from loopback_transport import send_frame, receive_frame
from test_native_local_bridge import NativeFixture, issue_certificate
from native_tls_receiver import NativeTLSLoopbackReceiver

WIRE = b"1|45|1|managed_runtime|arm|1"


class RuntimeBoundaryTests(unittest.TestCase):
    def test_parent_rejection_cannot_bind_or_advertise_listener(self):
        launch = ManagedBridgeLaunch("\\\\.\\pipe\\IDW-LocalBootstrap-" + "a" * 64, 123, 45)
        with patch("managed_windows_tls_runtime.launch_api.hold_launch_parent", side_effect=PermissionError), \
                patch("managed_windows_tls_runtime.NativeTLSLoopbackReceiver") as receiver:
            with self.assertRaises(PermissionError):
                serve_managed_windows_tls_once(launch, None, expected_peer_id="guest",
                    approved_client_sha256="a" * 64, host_consent=lambda _: True,
                    host_approval_ledger=NativeCommandApprovalLedger(),
                    on_listening=lambda _: self.fail("unverified launch must not advertise"))
            receiver.assert_not_called()

    def test_cancellation_at_serve_entry_is_graceful_and_disposes_owned_listener(self):
        launch = ManagedBridgeLaunch("\\\\.\\pipe\\IDW-LocalBootstrap-" + "a" * 64, 123, 45)
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2;context.verify_mode = ssl.CERT_REQUIRED
        receiver = NativeTLSLoopbackReceiver(context);cancel = threading.Event()
        self.addCleanup(receiver.close)
        original_serve = receiver.serve_once
        entered = []
        @contextmanager
        def hold(_):
            entered.append(True)
            try: yield 456
            finally: entered.pop()
        def serve(**options):
            # Precisely after the runtime's last startup cancellation check, but
            # before the receiver changes ready -> serving. A watcher must not
            # dispose the receiver in this window.
            cancel.set()
            until = time.monotonic() + 1
            while not receiver._cancel.is_set() and time.monotonic() < until: time.sleep(.005)
            self.assertTrue(receiver._cancel.is_set())
            self.assertEqual(entered, [True])
            return original_serve(**options)
        with patch("managed_windows_tls_runtime.launch_api.hold_launch_parent", side_effect=hold), \
                patch("managed_windows_tls_runtime.launch_api.launch_parent_is_alive", return_value=True), \
                patch("managed_windows_tls_runtime.NativeTLSLoopbackReceiver", return_value=receiver), \
                patch.object(receiver, "serve_once", side_effect=serve):
            self.assertEqual(serve_managed_windows_tls_once(launch, context, expected_peer_id="guest",
                approved_client_sha256="a" * 64, host_consent=lambda _: self.fail("canceled before consent"),
                host_approval_ledger=NativeCommandApprovalLedger(), on_listening=lambda _: None, cancel=cancel), 0)
        self.assertEqual(receiver._listener.fileno(), -1)
        self.assertEqual(entered, [])


class RuntimeFixture(NativeFixture):
    def __init__(self, config, fingerprint):
        self.native_lease = None
        self.process = subprocess.Popen([os.environ["IDW_NATIVE_LOCAL_FIXTURE"], "--runtime-child-host",
            sys.executable, str(Path(__file__).resolve()), str(config)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0,
            close_fds=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        try:
            self.process.stdin.write((fingerprint + "|" + "0" * 64 + "\n").encode());self.process.stdin.flush()
            ready = self._read()
            if not ready.startswith(b"TLS_PORT "): raise RuntimeError("runtime readiness unavailable")
            self.port = int(ready[9:])
            if not 1 <= self.port <= 65535: raise RuntimeError("invalid runtime port")
        except Exception:
            self.close();raise


@unittest.skipUnless(os.name == "nt" and os.environ.get("IDW_NATIVE_LOCAL_FIXTURE"), "Windows native host fixture required")
class ManagedRuntimeIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.cert, cls.key = issue_certificate(cls.folder.name, "localhost")
        cls.guest_cert, cls.guest_key = issue_certificate(cls.folder.name, "guest")
        cls.fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cls.guest_cert.read_text())).hexdigest()

    @classmethod
    def tearDownClass(cls): cls.folder.cleanup()

    def fixture(self, *, host_pin=None, child_pin=None, cancel_after_command=False):
        config = Path(self.folder.name) / "runtime-config.json"
        # Trusted test configuration contains paths/pins, never bootstrap credentials.
        config.write_text(json.dumps(dict(cert=str(self.cert), key=str(self.key),
            client_ca=str(self.guest_cert), client_pin=child_pin or self.fingerprint,
            cancel_after_command=cancel_after_command)))
        fixture = RuntimeFixture(config, host_pin or self.fingerprint)
        self.addCleanup(fixture.close)
        return fixture

    def connect(self, fixture, credentials=True):
        context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cafile=str(self.cert))
        if credentials: context.load_cert_chain(str(self.guest_cert), str(self.guest_key))
        client = context.wrap_socket(socket.create_connection(("127.0.0.1", fixture.port), timeout=3),
                                     server_hostname="localhost")
        client.settimeout(3);self.addCleanup(client.close);return client

    def wait_revoked(self, fixture):
        until = time.monotonic() + 3
        while time.monotonic() < until:
            if fixture.pending() == 0: return
            time.sleep(.025)
        self.fail("native pending review was not cleared")

    def test_actual_parent_tls_offer_native_review_and_eof_revocation(self):
        fixture = self.fixture();client = self.connect(fixture)
        send_frame(client, WIRE);self.assertEqual(receive_frame(client), WIRE)
        self.assertEqual(fixture.pending(), 1)
        client.shutdown(socket.SHUT_WR);self.wait_revoked(fixture)

    def test_payload_substitution_rejected_and_no_native_review_survives(self):
        fixture = self.fixture();client = self.connect(fixture)
        send_frame(client, b"1|45|1|managed_runtime|arm|2")
        with self.assertRaises(OSError): receive_frame(client)
        self.wait_revoked(fixture)

    def test_unapproved_host_pin_or_client_pin_fails_before_native_admission(self):
        for changes in (dict(host_pin="0" * 64), dict(child_pin="0" * 64)):
            with self.subTest(changes=tuple(changes)):
                fixture = self.fixture(**changes);client = self.connect(fixture)
                try: send_frame(client, WIRE)
                except OSError: pass
                with self.assertRaises(OSError): receive_frame(client)
                self.assertNotEqual(fixture.process.wait(timeout=5), 0)

    def test_host_exit_terminates_owned_runtime_and_tls_connection(self):
        fixture = self.fixture();client = self.connect(fixture)
        send_frame(client, WIRE);self.assertEqual(receive_frame(client), WIRE)
        self.assertEqual(fixture.pending(), 1)
        fixture.process.kill();fixture.process.wait(timeout=3)
        with self.assertRaises(OSError): receive_frame(client)

    def test_owner_cancellation_revokes_native_review_without_stdio_revoke(self):
        fixture = self.fixture(cancel_after_command=True);client = self.connect(fixture)
        send_frame(client, WIRE);self.assertEqual(receive_frame(client), WIRE)
        self.assertEqual(fixture.pending(), 1)
        started = time.monotonic();self.wait_revoked(fixture)
        self.assertLess(time.monotonic() - started, 2.5) # Earlier than the 3-second idle timeout.
        with self.assertRaises(OSError): receive_frame(client)


def runtime_child():
    launch = parse_managed_bridge_arguments(["--idw-native-offer", *sys.argv[2:5]])
    config = json.loads(Path(sys.argv[5]).read_text()) # Trusted test policy, not peer input.
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.verify_mode = ssl.CERT_REQUIRED
    context.load_cert_chain(config["cert"], config["key"]);context.load_verify_locations(cafile=config["client_ca"])
    ledger = NativeCommandApprovalLedger();calls = 0;cancel = threading.Event()
    def consent(identity):
        nonlocal calls
        calls += 1
        if calls > 1:
            ledger.approve(identity.peer_id, identity.epoch, identity.connection_id, WIRE)
        if calls == 3 and config.get("cancel_after_command"):
            timer = threading.Timer(1.0, cancel.set);timer.daemon = True;timer.start()
        return True # Explicit trusted test-host policy, never a remote approval.
    serve_managed_windows_tls_once(launch, context, expected_peer_id="guest",
        approved_client_sha256=config["client_pin"], host_consent=consent,
        host_approval_ledger=ledger, on_listening=lambda port: print("TLS_PORT", port, flush=True),
        session_timeout=15, command_timeout=3, cancel=cancel)


if __name__ == "__main__":
    if len(sys.argv) == 6 and sys.argv[1] == "--runtime-child": runtime_child()
    else: unittest.main()
