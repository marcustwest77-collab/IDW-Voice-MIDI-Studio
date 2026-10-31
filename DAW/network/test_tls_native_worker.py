"""mTLS client -> piped worker bootstrap -> native socket -> C++ event queue."""
import hashlib
import os
from pathlib import Path
import socket
import ssl
import subprocess
import tempfile
import time
import unittest
from tls_client import verified_tls_client
from tls_worker_bootstrap import TLSWorkerProcess, fresh_session_secret
from local_test_client import send_local_request


class TLSNativeWorkerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.folder.cleanup)
        cls.root = Path(cls.folder.name)
        for name in ("localhost", "guest"):
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                            "-keyout", str(cls.root / (name+".key")),
                            "-out", str(cls.root / (name+".crt")), "-days", "1",
                            "-subj", "/CN="+name, "-addext", "subjectAltName=DNS:"+name],
                           check=True, capture_output=True, timeout=25)
        cls.pin = hashlib.sha256(ssl.PEM_cert_to_DER_cert(
            (cls.root / "guest.crt").read_text())).hexdigest()

    def setUp(self):
        self.token = fresh_session_secret()
        self.native = subprocess.Popen([os.environ["IDW_NATIVE_RECEIVER_FIXTURE"]],
                                       stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                       text=True, bufsize=1)
        self.addCleanup(self.close_native)
        self.native.stdin.write(self.token+"\n")
        self.native.stdin.flush()
        self.native_port = int(self.native.stdout.readline())
        self.assertEqual(self.operation("admitted"), "no")

    def close_native(self):
        if self.native.poll() is None:
            self.native.kill()
        self.native.wait(timeout=5)
        self.native.stdin.close()
        self.native.stdout.close()

    def operation(self, command):
        self.native.stdin.write(command+"\n")
        self.native.stdin.flush()
        return self.native.stdout.readline().strip()

    def config(self, **overrides):
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            tls_port = probe.getsockname()[1]
        config = dict(version=1, native_port=self.native_port, tls_port=tls_port,
                      token=self.token, peer="peer", epoch=45,
                      channel="test_channel_binding_12345",
                      server_cert=str(self.root / "localhost.crt"),
                      server_key=str(self.root / "localhost.key"),
                      client_ca=str(self.root / "guest.crt"), client_pin=self.pin,
                      host_approved=True, track_control=True, approved_requests=["arm1"])
        config.update(overrides)
        return config

    def exchange(self, payload=b"1|45|1|arm1|arm|0", **overrides):
        config = self.config(**overrides)
        with TLSWorkerProcess(config) as worker:
            self.assertNotIn(self.token, repr(worker.process.args))
            try:
                reply = verified_tls_client("127.0.0.1", config["tls_port"], "localhost",
                                            config["server_cert"], payload,
                                            client_certfile=str(self.root / "guest.crt"),
                                            client_keyfile=str(self.root / "guest.key"))
            except (ConnectionError, ssl.SSLError, OSError):
                reply = None
            code = worker.process.wait(timeout=6)
            self.assertEqual(worker.process.stdout.read(), b"")
            return reply, code

    def test_success_and_replay_after_worker_restart(self):
        payload = b"1|45|1|arm1|arm|0"
        self.assertEqual(self.exchange(payload), (payload, 0))
        self.assertEqual(self.operation("admitted"), "yes")
        self.assertEqual(self.operation("drain"), "1")
        self.assertEqual(self.exchange(payload), (None, 1))
        self.assertEqual(self.operation("drain"), "0")
        payload = b"1|45|2|arm1|select|1"
        self.assertEqual(self.exchange(payload), (payload, 0))
        self.assertEqual(self.operation("drain"), "1")

    def test_rejects_wrong_pin_token_peer_and_missing_approval(self):
        for change in ({"client_pin": "0"*64}, {"token": fresh_session_secret()},
                       {"peer": "other"}, {"approved_requests": ["different"]}):
            with self.subTest(fields=list(change)):
                self.assertEqual(self.exchange(**change), (None, 1))
                if "approved_requests" not in change:
                    self.assertEqual(self.operation("admitted"), "no")
                self.assertEqual(self.operation("drain"), "0")

    def test_revocation_and_forbidden_action(self):
        self.assertEqual(self.exchange(b"1|45|1|arm1|record|0"), (None, 1))
        self.assertEqual(self.operation("drain"), "0")
        self.assertEqual(self.operation("revoke"), "revoked")
        self.assertEqual(self.exchange(), (None, 1))
        self.assertEqual(self.operation("drain"), "0")

    def test_client_certificate_is_required(self):
        config = self.config()
        with TLSWorkerProcess(config) as worker:
            with self.assertRaises((ConnectionError, ssl.SSLError, OSError)):
                verified_tls_client("127.0.0.1", config["tls_port"], "localhost",
                                    config["server_cert"], b"1|45|1|arm1|arm|0")
            self.assertEqual(worker.process.wait(timeout=6), 1)
        self.assertEqual(self.operation("admitted"), "no")
        self.assertEqual(self.operation("drain"), "0")

    def test_native_unavailable_does_not_report_success(self):
        self.assertEqual(self.operation("stop"), "stopped")
        self.native.wait(timeout=3)
        self.assertEqual(self.exchange(), (None, 1))

    def test_parent_can_stop_idle_worker(self):
        worker = TLSWorkerProcess(self.config())
        self.addCleanup(worker.close)
        start = time.monotonic()
        worker.close()
        self.assertIsNotNone(worker.process.poll())
        self.assertLess(time.monotonic()-start, 2.5)
        self.assertEqual(self.operation("drain"), "0")

    def test_local_client_model_delivers_for_host_review(self):
        config=self.config()
        setup={key:config[key] for key in ("version","peer","tls_port","server_cert",
                                           "server_key","client_ca","client_pin","approved_requests")}
        with TLSWorkerProcess(config) as worker:
            result=send_local_request(setup,str(self.root/"guest.crt"),str(self.root/"guest.key"),
                                      epoch=45,sequence=1,request_id="arm1",action="select",track_number=1)
            self.assertIn("queued",result)
            self.assertEqual(worker.process.wait(timeout=6),0)
        self.assertEqual(self.operation("admitted"),"yes")
        self.assertEqual(self.operation("drain"),"1")


if __name__ == "__main__":
    unittest.main()
