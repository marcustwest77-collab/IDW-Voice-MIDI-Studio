"""End-to-end *local* TLS guarded echo tests with development certificates.

Inputs representing user identity and channel token are synthetic test fixtures,
not derived from the TLS connection. No DAW control is performed.
"""
import shutil
import hashlib
import socket
import ssl
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

from channel_bound_session import BoundSession
from replay_guard import ReplayGuard
from host_approval_ledger import HostApprovalLedger
from tls_client import verified_tls_client
from tls_loopback_server import serve_tls_once


@unittest.skipUnless(shutil.which("openssl"), "openssl required")
class GuardedTLSRoundtripTest(unittest.TestCase):
    def test_authorized_and_unauthorized_command(self):
        with tempfile.TemporaryDirectory() as folder:
            cert = Path(folder) / "localhost.crt"
            key = Path(folder) / "localhost.key"
            subprocess.run(
                ["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                 "-keyout", str(key), "-out", str(cert), "-days", "1",
                 "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost"],
                check=True, capture_output=True, timeout=25,
            )
            client_cert = Path(folder) / "guest.crt"
            client_key = Path(folder) / "guest.key"
            subprocess.run(
                ["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                 "-keyout", str(client_key), "-out", str(client_cert), "-days", "1",
                 "-subj", "/CN=guest", "-addext", "subjectAltName=DNS:guest"],
                check=True, capture_output=True, timeout=25,
            )
            fingerprint = hashlib.sha256(
                ssl.PEM_cert_to_DER_cert(client_cert.read_text())
            ).hexdigest()
            session = BoundSession("guest", 45, "test_channel_binding_12345")
            self.assertTrue(session.admit_bound(
                "guest", "test_channel_binding_12345", True, True
            ))
            self.assertTrue(session.grant(control=True, recording=False))
            guard = ReplayGuard()
            ledger = HostApprovalLedger()

            def exchange(payload, mapped_peer='guest', host_allowed=True):
                with socket.socket() as probe:
                    probe.bind(("127.0.0.1", 0))
                    port = probe.getsockname()[1]
                ready = threading.Event()
                server_failures = []

                def run_server():
                    try:
                        serve_tls_once(port, str(cert), str(key), ready=ready,
                                       session=session, verified_peer_id="guest",
                                       verified_channel_id="test_channel_binding_12345",
                                       replay_guard=guard, client_cafile=str(client_cert),
                                       approved_client_sha256=fingerprint,
                                       approved_peer_by_sha256={fingerprint: mapped_peer},
                                       host_approval=lambda peer, epoch: host_allowed and peer == 'guest' and epoch == 45,
                                       host_approval_ledger=ledger)
                    except Exception as exc:
                        server_failures.append(exc)

                worker = threading.Thread(target=run_server, daemon=True)
                worker.start()
                self.assertTrue(ready.wait(5))
                try:
                    reply = verified_tls_client(
                        "127.0.0.1", port, "localhost", str(cert), payload,
                        client_certfile=str(client_cert), client_keyfile=str(client_key)
                    )
                except (ConnectionError, ssl.SSLError, OSError):
                    reply = None
                worker.join(timeout=5)
                self.assertFalse(worker.is_alive())
                return reply, server_failures

            denied, errors = exchange(b"1|45|1|wrong_guest|arm|1", mapped_peer="intruder")
            self.assertIsNone(denied)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))
            denied, errors = exchange(b"1|45|1|host_denied|arm|1", host_allowed=False)
            self.assertIsNone(denied)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))
            ledger.approve("guest", 45, "arm_one")
            allowed, errors = exchange(b"1|45|1|arm_one|arm|1")
            self.assertEqual(allowed, b"1|45|1|arm_one|arm|1")
            self.assertFalse(errors)
            denied, errors = exchange(b"1|45|2|not_approved|arm|1")
            self.assertIsNone(denied)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))
            # Denied request must not consume sequence 2; a later approved
            # command can use it without weakening replay prevention.
            ledger.approve("guest", 45, "select_two")
            allowed, errors = exchange(b"1|45|2|select_two|select|1")
            self.assertEqual(allowed, b"1|45|2|select_two|select|1")
            self.assertFalse(errors)
            ledger.approve("guest", 45, "record_two")
            denied, errors = exchange(b"1|45|2|record_two|record|1")
            self.assertIsNone(denied)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))
            denied, errors = exchange(b"1|45|1|arm_one|arm|1")
            self.assertIsNone(denied)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))


if __name__ == "__main__":
    unittest.main()
