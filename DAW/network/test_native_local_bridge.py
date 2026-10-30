"""Local native handoff and pinned mTLS integration; not a two-machine test."""
from dataclasses import replace
import datetime
import hashlib
import importlib.util
import os
from pathlib import Path
import queue
import secrets
import shutil
import socket
import ssl
import subprocess
import tempfile
import threading
import unittest
from unittest.mock import MagicMock, patch

from channel_bound_session import BoundSession
from host_approval_ledger import HostApprovalLedger
from native_local_bridge import NativeLocalBootstrap, NativeLocalCommandBridge
from replay_guard import ReplayGuard
from tls_client import verified_tls_client
from tls_loopback_server import serve_tls_once
from verified_event_producer import VerifiedCommandEvent, VerifiedEventProducer


def binding(bootstrap, **changes):
    arguments = dict(expected_peer_id="guest", verified_peer_sha256=bootstrap.peer_sha256,
                     verified_connection_id=bootstrap.connection_id)
    arguments.update(changes)
    return NativeLocalCommandBridge(bootstrap, **arguments)


class NativeBridgeUnitTests(unittest.TestCase):
    def setUp(self):
        self.bootstrap = NativeLocalBootstrap(12345, 45, "1" * 64, "a" * 64, "b" * 64)
        self.event = VerifiedCommandEvent(45, "1|45|1|test|arm|1", "guest")

    def test_decode_exact_native_record_and_redact(self):
        packet = b"IDWB\x01" + (12345).to_bytes(2, "big") + (45).to_bytes(8, "big") + b"1" * 64 + b"a" * 64 + b"b" * 64
        self.assertEqual(NativeLocalBootstrap.decode(packet), self.bootstrap)
        self.assertNotIn("1" * 64, repr(self.bootstrap))
        for bad in (packet[:-1], packet + b"x", b"X" + packet[1:], packet[:4] + b"\x02" + packet[5:],
                    packet[:5] + b"\x00\x00" + packet[7:], packet[:7] + b"\x00" * 8 + packet[15:],
                    packet[:15] + b"\xff" + packet[16:]):
            with self.assertRaisesRegex(ValueError, "invalid native bootstrap"):
                NativeLocalBootstrap.decode(bad)

    def test_mismatched_binding_and_invalid_timeout_fail(self):
        for changes in (dict(verified_peer_sha256="c" * 64), dict(verified_connection_id="c" * 64),
                        dict(expected_peer_id=""), *(dict(timeout=value) for value in
                          (0, 6, True, float("nan"), float("inf"), 10**1000))):
            with self.assertRaises(ValueError):
                binding(self.bootstrap, **changes)

    def test_invalid_events_never_open_socket(self):
        bridge = binding(self.bootstrap)
        cases = [None, replace(self.event, peer_id="intruder"), replace(self.event, epoch=46),
                 replace(self.event, epoch=True), replace(self.event, wire="1|46|1|x|arm|1"),
                 replace(self.event, wire="1|45|1|x|record|1"), replace(self.event, wire="1|45|1|x|arm|128"),
                 replace(self.event, wire="1|45|01|x|arm|1"), replace(self.event, wire="broken"),
                 replace(self.event, wire="1|45|1|non_\u00e9|arm|1")]
        with patch("native_local_bridge.socket.create_connection") as connect:
            for event in cases:
                self.assertFalse(bridge.deliver(event))
            connect.assert_not_called()

    def test_native_envelope_fin_receipt_and_close(self):
        bridge = binding(self.bootstrap)
        connection = MagicMock()
        connection.__enter__.return_value = connection
        connection.recv.return_value = b"1"
        with patch("native_local_bridge.socket.create_connection", return_value=connection) as connect:
            self.assertTrue(bridge.deliver(self.event))
            self.assertEqual(connect.call_args.args[0], ("127.0.0.1", 12345))
            packet = connection.sendall.call_args.args[0]
            self.assertEqual(int.from_bytes(packet[:4], "big"), len(packet) - 4)
            self.assertEqual(packet[4:], b"1" * 64 + b"\n" + self.event.wire.encode("ascii"))
            connection.shutdown.assert_called_once_with(socket.SHUT_WR)
            for reply in (b"0", b"", b"2"):
                connection.recv.return_value = reply
                self.assertFalse(bridge.deliver(self.event))
            bridge.close()
            connect.reset_mock()
            self.assertFalse(bridge.deliver(self.event))
            connect.assert_not_called()

    def test_one_deadline_covers_all_phases(self):
        connection = MagicMock()
        connection.__enter__.return_value = connection
        with patch("native_local_bridge.socket.create_connection", return_value=connection), \
                patch("native_local_bridge.time.monotonic", side_effect=[0, .1, .4, .8, 1.1]):
            self.assertFalse(binding(self.bootstrap).deliver(self.event))
        connection.recv.assert_not_called()


class NativeFixture:
    """Test-only stdio control. Credentials stay in private captured pipes."""
    def __init__(self, fingerprint, channel):
        self.process = subprocess.Popen([os.environ["IDW_NATIVE_LOCAL_FIXTURE"]],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0,
            close_fds=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        try:
            self.process.stdin.write((fingerprint + "|" + channel + "\n").encode("ascii"))
            self.process.stdin.flush()
            self.bootstrap = NativeLocalBootstrap.decode(self._read(207))
        except Exception:
            self.close()
            raise

    def _read(self, count=None):
        result = queue.Queue()
        def read():
            if count is None:
                data = self.process.stdout.readline(128)
            else:
                data = b""
                while len(data) < count:
                    chunk = self.process.stdout.read(count - len(data))
                    if not chunk:
                        break
                    data += chunk
            result.put(data)
        worker = threading.Thread(target=read, daemon=True)
        worker.start()
        try:
            data = result.get(timeout=5)
        except queue.Empty:
            self.close()
            raise RuntimeError("native fixture response timed out") from None
        worker.join(timeout=1)
        if not data or (count is not None and len(data) != count):
            raise RuntimeError("native fixture response unavailable")
        return data

    def pending(self, command="DRAIN"):
        self.process.stdin.write((command + "\n").encode("ascii"))
        self.process.stdin.flush()
        response = self._read()
        if not response.startswith(b"PENDING ") or not response.endswith(b"\n"):
            raise RuntimeError("invalid native fixture status")
        return int(response[8:-1])

    def close(self):
        if self.process.poll() is None:
            try:
                self.process.stdin.write(b"QUIT\n")
                self.process.stdin.flush()
                self.process.wait(timeout=5)
            except (OSError, subprocess.TimeoutExpired):
                self.process.kill()
                self.process.wait(timeout=5)
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            stream.close()


@unittest.skipUnless(os.environ.get("IDW_NATIVE_LOCAL_FIXTURE"), "native fixture required")
class NativeQueueIntegrationTests(unittest.TestCase):
    def test_replay_wrong_secret_live_state_and_revocation(self):
        fixture = NativeFixture("a" * 64, "b" * 64)
        self.addCleanup(fixture.close)
        bridge = binding(fixture.bootstrap)
        self.addCleanup(bridge.close)
        event = VerifiedCommandEvent(45, "1|45|1|first|arm|1", "guest")
        self.assertEqual(fixture.pending(), 0)
        self.assertTrue(bridge.deliver(event))
        self.assertEqual(fixture.pending(), 1)
        self.assertFalse(bridge.deliver(event))
        wrong = binding(replace(fixture.bootstrap, _secret="0" * 64))
        self.addCleanup(wrong.close)
        self.assertFalse(wrong.deliver(replace(event, wire="1|45|2|bad_secret|arm|1")))
        self.assertEqual(fixture.pending("BUSY"), 1)
        self.assertTrue(bridge.deliver(replace(event, wire="1|45|2|busy|arm|1")))
        self.assertEqual(fixture.pending(), 1)  # Queue receipt is not host approval.
        self.assertEqual(fixture.pending("IDLE"), 1)
        self.assertTrue(bridge.deliver(replace(event, wire="1|45|3|select|select|0")))
        self.assertEqual(fixture.pending(), 2)
        self.assertEqual(fixture.pending("REVOKE"), 0)
        self.assertFalse(bridge.deliver(replace(event, wire="1|45|4|late|arm|1")))


def issue_certificate(folder, name):
    cert, key = Path(folder) / (name + ".crt"), Path(folder) / (name + ".key")
    if shutil.which("openssl"):
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
            "-keyout", str(key), "-out", str(cert), "-days", "1", "-subj", "/CN=" + name,
            "-addext", "subjectAltName=DNS:" + name], check=True, capture_output=True, timeout=25)
    else:  # Optional local test fallback; CI uses its existing OpenSSL CLI.
        from cryptography import x509
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.asymmetric import rsa
        from cryptography.x509.oid import NameOID
        private = rsa.generate_private_key(public_exponent=65537, key_size=2048)
        subject = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, name)])
        now = datetime.datetime.now(datetime.timezone.utc)
        certificate = (x509.CertificateBuilder().subject_name(subject).issuer_name(subject)
            .public_key(private.public_key()).serial_number(x509.random_serial_number())
            .not_valid_before(now - datetime.timedelta(minutes=1)).not_valid_after(now + datetime.timedelta(days=1))
            .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
            .add_extension(x509.SubjectAlternativeName([x509.DNSName(name)]), critical=False)
            .sign(private, hashes.SHA256()))
        cert.write_bytes(certificate.public_bytes(serialization.Encoding.PEM))
        key.write_bytes(private.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.TraditionalOpenSSL,
                                             serialization.NoEncryption()))
    return cert, key


@unittest.skipUnless(os.environ.get("IDW_NATIVE_LOCAL_FIXTURE") and
    (shutil.which("openssl") or importlib.util.find_spec("cryptography")), "native fixture and certificate tool required")
class NativeMTLSIntegrationTests(unittest.TestCase):
    def test_pinned_mtls_through_native_queue_into_pending_review(self):
        with tempfile.TemporaryDirectory() as folder:
            cert, key = issue_certificate(folder, "localhost")
            guest_cert, guest_key = issue_certificate(folder, "guest")
            other_cert, _ = issue_certificate(folder, "other")
            fingerprint = hashlib.sha256(ssl.PEM_cert_to_DER_cert(guest_cert.read_text())).hexdigest()
            other_pin = hashlib.sha256(ssl.PEM_cert_to_DER_cert(other_cert.read_text())).hexdigest()
            channel = secrets.token_hex(32)  # Synthetic trusted connection ID; not a TLS exporter.
            fixture = NativeFixture(fingerprint, channel)
            self.addCleanup(fixture.close)
            bridge = binding(fixture.bootstrap)
            self.addCleanup(bridge.close)
            session = BoundSession("guest", 45, channel)
            self.assertTrue(session.admit_bound("guest", channel, True, True))
            self.assertTrue(session.grant(control=True, recording=False))
            guard, ledger, handoffs = ReplayGuard(), HostApprovalLedger(), []
            def deliver(event):
                handoffs.append(event.wire)
                return bridge.deliver(event)
            producer = VerifiedEventProducer("guest", 45, deliver)

            def exchange(wire, *, pin=fingerprint, mapped="guest", allowed=True, credentials=True):
                with socket.socket() as probe:
                    probe.bind(("127.0.0.1", 0))
                    port = probe.getsockname()[1]
                ready, failures = threading.Event(), []
                def server():
                    try:
                        serve_tls_once(port, str(cert), str(key), ready=ready, session=session,
                            verified_channel_id=channel, replay_guard=guard, client_cafile=str(guest_cert),
                            approved_client_sha256=pin, approved_peer_by_sha256={fingerprint: mapped},
                            host_approval=lambda peer, epoch: allowed and peer == "guest" and epoch == 45,
                            host_approval_ledger=ledger, accepted_command_sink=producer.accepted_command_sink)
                    except Exception as error:
                        failures.append(error)
                worker = threading.Thread(target=server, daemon=True)
                worker.start()
                self.assertTrue(ready.wait(5))
                try:
                    reply = verified_tls_client("127.0.0.1", port, "localhost", str(cert), wire,
                        client_certfile=str(guest_cert) if credentials else None,
                        client_keyfile=str(guest_key) if credentials else None)
                except (ConnectionError, OSError):
                    reply = None
                worker.join(timeout=5)
                self.assertFalse(worker.is_alive())
                return reply, failures

            self.assertEqual(fixture.pending(), 0)
            for changes in (dict(credentials=False), dict(pin=other_pin), dict(mapped="intruder"), dict(allowed=False), {}):
                reply, failures = exchange(b"1|45|1|unapproved|arm|1", **changes)
                self.assertIsNone(reply)
                self.assertTrue(failures)
                self.assertEqual(fixture.pending(), 0)
            self.assertEqual(handoffs, [])
            ledger.approve("guest", 45, "record")
            self.assertIsNone(exchange(b"1|45|1|record|record|1")[0])
            self.assertTrue(ledger.contains("guest", 45, "record"))
            for sequence, request, action, pending in ((1, "first", "arm", 1), (2, "second", "select", 2)):
                ledger.approve("guest", 45, request)
                wire = f"1|45|{sequence}|{request}|{action}|1".encode("ascii")
                reply, failures = exchange(wire)
                self.assertEqual(reply, wire)
                self.assertEqual(failures, [])
                self.assertEqual(fixture.pending(), pending)
            ledger.approve("guest", 45, "replay")
            self.assertIsNone(exchange(b"1|45|1|replay|arm|1")[0])
            self.assertTrue(ledger.contains("guest", 45, "replay"))
            self.assertIsNone(exchange(b"malformed")[0])
            self.assertEqual(fixture.pending("BUSY"), 2)
            ledger.approve("guest", 45, "busy")
            self.assertEqual(exchange(b"1|45|3|busy|arm|1")[0], b"1|45|3|busy|arm|1")
            self.assertEqual(fixture.pending(), 2)  # Fresh native DAW state rejects arming.
            self.assertEqual(fixture.pending("REVOKE"), 0)
            ledger.approve("guest", 45, "after_revoke")
            self.assertIsNone(exchange(b"1|45|4|after_revoke|arm|1")[0])
            self.assertEqual(fixture.pending(), 0)


if __name__ == "__main__":
    unittest.main()
