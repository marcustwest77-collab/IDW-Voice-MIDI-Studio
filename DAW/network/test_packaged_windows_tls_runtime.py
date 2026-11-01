"""Windows CI only: freeze a TEST child and launch it via the pinned manager.

This is neither a production package nor a beta installation. Its TLS material,
readiness file and approvals are owned exclusively by this temporary test run.
"""
import hashlib
import os
import re
from pathlib import Path
import socket
import ssl
import subprocess
import sys
import tempfile
import time
import unittest

from loopback_transport import send_frame, receive_frame
from native_tls_client import connect_native_tls_client
from native_tls_command_client import open_native_command_client, NativeQueueReceipt
from test_native_local_bridge import NativeFixture, issue_certificate

WIRE = b"1|45|1|packaged_runtime|arm|1"


class ManagedHostFixture(NativeFixture):
    def __init__(self, executable, digest, peer_pin, *, ready_profile=False):
        self.native_lease = None
        self.peer_pin = peer_pin
        self.ready_profile = ready_profile
        self.process = subprocess.Popen([os.environ["IDW_MANAGED_TLS_FIXTURE"],
            *(["--ready-profile"] if ready_profile else []), str(executable)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0,
            close_fds=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        self.process.stdin.write((digest + "|" + peer_pin + "\n").encode("ascii"))
        self.process.stdin.flush()

    def decide(self, *, allow=True, substitute=False, delay=0):
        prompt = self._read(142) # Fixed test epoch 45 + exact pin/nonce/version record.
        match = re.fullmatch(rb"OFFER (45\|([0-9a-f]{64})\|[0-9a-f]{64}\|1[23])\n", prompt)
        if match is None or match[2].decode("ascii") != self.peer_pin:
            raise RuntimeError("untrusted native offer prompt")
        binding = match[1]
        if delay: time.sleep(delay) # Trusted test controller simulates reading the certificate.
        if substitute: binding = binding.replace(b"45|", b"46|", 1)
        self.process.stdin.write((b"ALLOW|" if allow else b"DENY|") + binding + b"\n")
        self.process.stdin.flush()


@unittest.skipUnless(os.name == "nt" and os.environ.get("IDW_MANAGED_TLS_FIXTURE") and
                     os.environ.get("IDW_SYNTHETIC_BRIDGE_FIXTURE"), "native smoke fixtures required")
class NativeManagedHostSmokeTests(unittest.TestCase):
    def test_actual_manager_fixture_review_and_stop_with_synthetic_tls_evidence(self):
        # This local smoke test does NOT authenticate TLS; the packaged suite below does.
        bridge = Path(os.environ["IDW_SYNTHETIC_BRIDGE_FIXTURE"]).resolve()
        host = ManagedHostFixture(bridge, hashlib.sha256(bridge.read_bytes()).hexdigest(), "a" * 64)
        self.addCleanup(host.close)
        host.decide()
        self.assertEqual(host._read(), b"ACTIVE\n")
        self.assertEqual(host.pending(), 1)
        self.assertEqual(host.pending("STOP"), 0)


@unittest.skipUnless(os.name == "nt" and os.environ.get("IDW_PACKAGED_TLS_TESTS") == "1",
                     "explicit Windows packaging CI required")
class PackagedManagedTLSIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory(prefix="idw-packaged-test-")
        cls.addClassCleanup(cls.folder.cleanup)
        root = Path(cls.folder.name)
        cls.cert, cls.key = issue_certificate(root, "localhost")
        cls.client_cert, cls.client_key = issue_certificate(root, "guest")
        cls.pin = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cls.client_cert.read_text())).hexdigest()
        cls.ready = root / "ready.txt"
        policy = dict(cert=str(cls.cert), key=str(cls.key), client_ca=str(cls.client_cert),
                      client_pin=cls.pin, ready=str(cls.ready))
        entry = root / "fixture_entry.py"
        entry.write_text("from packaged_tls_child_fixture import run_fixture\n"
                         "if __name__ == '__main__':\n    run_fixture(" + repr(policy) + ")\n", encoding="utf-8")
        # The Codex-managed Python runtime loads this additional certificate
        # distrust policy when constructing SSLContext. Preserve it in frozen
        # builds rather than editing ssl.py or weakening the runtime policy.
        runtime_policy = Path(ssl.__file__).with_name("codex-rejected-cas.pem")
        runtime_data = (["--add-data", str(runtime_policy) + os.pathsep + "."]
                        if runtime_policy.is_file() else [])
        # Windows onedir keeps the TLS owner in the actual directly launched
        # process. Onefile's extra bootloader parent is not this launch contract.
        result = subprocess.run([sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean", "--onedir",
            "--name", "IDWPackagedTLSFixture", "--paths", str(Path(__file__).resolve().parent),
            "--distpath", str(root / "dist"), "--workpath", str(root / "build"),
            "--specpath", str(root), *runtime_data, str(entry)], capture_output=True, timeout=180,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        if result.returncode:
            raise RuntimeError("test-only package build failed: " + result.stderr.decode(errors="replace")[-6000:])
        cls.executable = root / "dist/IDWPackagedTLSFixture/IDWPackagedTLSFixture.exe"
        cls.digest = hashlib.sha256(cls.executable.read_bytes()).hexdigest() # Explicit TEST approval only.

    def host(self, *, digest=None, ready_profile=False):
        self.ready.unlink(missing_ok=True)
        host = ManagedHostFixture(self.executable, digest or self.digest, self.pin, ready_profile=ready_profile)
        self.addCleanup(host.close)
        return host

    def discovery_port(self, host):
        deadline = time.monotonic() + 6
        if host.ready_profile:
            record = host._read()
            self.assertRegex(record, rb"^READY [1-9][0-9]{0,4}\n$")
            port = int(record[6:]);self.assertFalse(self.ready.exists())
        else:
            while not self.ready.exists():
                if host.process.poll() is not None:
                    self.fail("pinned managed host exited before test readiness")
                if time.monotonic() >= deadline: self.fail("packaged TLS child did not become ready")
                time.sleep(.01)
            port = int(self.ready.read_text(encoding="ascii"))
        self.assertTrue(1 <= port <= 65535)
        return port

    def tls_client(self, host, *, credentials=True):
        port=self.discovery_port(host)
        if credentials:
            return self.enterContext(connect_native_tls_client(port,
                approved_server_sha256=hashlib.sha256(ssl.PEM_cert_to_DER_cert(self.cert.read_text())).hexdigest(),
                server_ca_pem=self.cert.read_text(), certificate_file=self.client_cert,
                private_key_file=self.client_key, timeout=3))
        context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cafile=str(self.cert))
        # Deliberately credential-free client for the negative mTLS test only.
        client = context.wrap_socket(socket.create_connection(("127.0.0.1", port), timeout=3),
                                     server_hostname="localhost")
        client.settimeout(3);self.addCleanup(client.close)
        return client

    def connect(self, host, *, approve=True, substitute=False, decision_delay=0):
        client = self.tls_client(host)
        host.decide(allow=approve, substitute=substitute, delay=decision_delay)
        if approve and not substitute: self.assertEqual(host._read(), b"ACTIVE\n")
        return client

    def command(self, host, client):
        send_frame(client, WIRE)
        self.assertEqual(receive_frame(client), WIRE)
        self.assertEqual(host.pending(), 1)

    def test_packaged_child_actual_manager_tls_native_review_and_eof(self):
        host = self.host();client = self.connect(host);self.command(host, client)
        client.shutdown(socket.SHUT_WR)
        deadline = time.monotonic() + 3
        while host.pending() != 0:
            if time.monotonic() >= deadline: self.fail("EOF did not revoke native pending review")
            time.sleep(.025)

    def test_private_ready_pipe_survives_tls_and_bootstrap_without_a_readiness_file(self):
        host = self.host(ready_profile=True);client = self.connect(host);self.command(host, client)
        self.assertFalse(self.ready.exists())
        client.shutdown(socket.SHUT_WR)
        deadline = time.monotonic()+3
        while host.pending() != 0:
            if time.monotonic() >= deadline: self.fail("private readiness TLS EOF was not revoked")
            time.sleep(.025)
        self.assertFalse(self.ready.exists())

    def test_ready_metadata_without_a_client_certificate_never_reaches_consent(self):
        host = self.host(ready_profile=True)
        with self.assertRaises(OSError):
            client = self.tls_client(host, credentials=False)
            send_frame(client, WIRE);receive_frame(client)
        self.assertEqual(host.process.wait(timeout=5), 4)
        self.assertNotIn(b"OFFER", host.process.stdout.read())
        self.assertFalse(self.ready.exists())

    def test_bounded_command_client_returns_queue_receipt_then_revokes_on_exit(self):
        host=self.host(ready_profile=True)
        with open_native_command_client(self.discovery_port(host),epoch=45,
                approved_server_sha256=hashlib.sha256(ssl.PEM_cert_to_DER_cert(self.cert.read_text())).hexdigest(),
                server_ca_pem=self.cert.read_text(),certificate_file=self.client_cert,
                private_key_file=self.client_key,command_timeout=3) as client:
            host.decide();self.assertEqual(host._read(),b"ACTIVE\n")
            receipt=client.submit(WIRE)
            self.assertIsInstance(receipt,NativeQueueReceipt)
            self.assertEqual((receipt.epoch,receipt.sequence),(45,1))
            self.assertEqual(host.pending(),1) # Pending host review, not executed.
            with self.assertRaises(ValueError): client.submit(WIRE)
            self.assertEqual(host.pending(),1)
        deadline=time.monotonic()+3
        while host.pending()!=0:
            if time.monotonic()>=deadline: self.fail("command-client exit did not revoke pending review")
            time.sleep(.025)
        self.assertFalse(self.ready.exists())

    def test_manager_stop_clears_pending_review_and_terminates_tls_child(self):
        host = self.host();client = self.connect(host);self.command(host, client)
        self.assertEqual(host.pending("STOP"), 0)
        with self.assertRaises(OSError): receive_frame(client)

    def test_human_consent_after_three_seconds_still_requires_exact_binding(self):
        host = self.host();client = self.connect(host, decision_delay=3)
        self.command(host, client)
        self.assertEqual(host.pending("STOP"), 0)

    def test_unapproved_executable_digest_fails_before_readiness(self):
        host = self.host(digest="0" * 64)
        self.assertEqual(host.process.wait(timeout=5), 4)
        self.assertFalse(self.ready.exists())

    def test_denied_or_substituted_native_consent_never_activates_session(self):
        for approve, substitute in ((False, False), (True, True)):
            with self.subTest(approve=approve, substitute=substitute):
                host = self.host();client = self.connect(host, approve=approve, substitute=substitute)
                self.assertEqual(host.process.wait(timeout=5), 4)
                self.assertNotIn(b"ACTIVE", host.process.stdout.read())
                with self.assertRaises(OSError): receive_frame(client)


if __name__ == "__main__": unittest.main()
