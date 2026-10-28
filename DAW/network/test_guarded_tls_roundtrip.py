"""End-to-end *local* TLS guarded echo tests with development certificates.

Inputs representing user identity and channel token are synthetic test fixtures,
not derived from the TLS connection. No DAW control is performed.
"""
import shutil
import hashlib
import os
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
            received_commands = []
            cpp_results = []
            fixture = os.environ.get('IDW_CPP_REVIEW_FIXTURE')
            cpp_process = None
            if fixture:
                cpp_process = subprocess.Popen(
                    [fixture], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE, text=True, bufsize=1)
                def close_fixture():
                    if cpp_process.poll() is None:
                        cpp_process.stdin.close()
                        try:
                            cpp_process.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            cpp_process.kill()
                            cpp_process.wait(timeout=5)
                    cpp_process.stdout.close()
                    cpp_process.stderr.close()
                self.addCleanup(close_fixture)
            def handoff(cmd, peer):
                received_commands.append((cmd, peer))
                if cpp_process is not None:
                    wire = (f"1|{cmd['session_epoch']}|{cmd['sequence']}|"
                            f"{cmd['request_id']}|{cmd['action']}|{cmd['track_index']}\n")
                    cpp_process.stdin.write(wire)
                    cpp_process.stdin.flush()
                    decision = cpp_process.stdout.readline().strip()
                    cpp_results.append(decision)
                    if decision != "REVIEW":
                        raise PermissionError("C++ host review rejected command")

            def exchange(payload, mapped_peer='guest', host_allowed=True, sink_override=None):
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
                                       host_approval_ledger=ledger,
                                       accepted_command_sink=handoff if sink_override is None else sink_override)
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
            # A receiver rejection must prevent a success echo to the TLS client.
            def reject_handoff(_command, _peer):
                raise PermissionError("C++ review denied")
            ledger.approve("guest", 45, "denied_by_cpp")
            denied, errors = exchange(
                b"1|45|1|denied_by_cpp|arm|1", sink_override=reject_handoff
            )
            self.assertIsNone(denied)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))
            ledger.approve("guest", 45, "arm_one")
            allowed, errors = exchange(b"1|45|2|arm_one|arm|1")
            self.assertEqual(allowed, b"1|45|2|arm_one|arm|1")
            self.assertFalse(errors)
            self.assertEqual(len(received_commands), 1)
            self.assertEqual(received_commands[0][0]["request_id"], "arm_one")
            self.assertEqual(received_commands[0][1], "guest")
            if os.environ.get("IDW_CPP_REVIEW_FIXTURE"):
                self.assertEqual(cpp_results, ["REVIEW"])
            denied, errors = exchange(b"1|45|3|not_approved|arm|1")
            self.assertIsNone(denied)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))
            # Denied request must not consume sequence 3; a later approved
            # command can use it without weakening replay prevention.
            ledger.approve("guest", 45, "select_two")
            allowed, errors = exchange(b"1|45|3|select_two|select|1")
            self.assertEqual(allowed, b"1|45|3|select_two|select|1")
            self.assertFalse(errors)
            if os.environ.get("IDW_CPP_REVIEW_FIXTURE"):
                self.assertEqual(cpp_results, ["REVIEW", "REVIEW"])
            ledger.approve("guest", 45, "record_two")
            denied, errors = exchange(b"1|45|4|record_two|record|1")
            self.assertIsNone(denied)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))
            # Forbidden actions cannot spend host approval ledger entries.
            self.assertTrue(ledger.contains("guest", 45, "record_two"))
            denied, errors = exchange(b"1|45|2|arm_one|arm|1")
            self.assertIsNone(denied)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))


if __name__ == "__main__":
    unittest.main()
