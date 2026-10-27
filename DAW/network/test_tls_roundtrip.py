"""True localhost TLS echo integration check using ephemeral OpenSSL certificates.

Only tests a single local connection; never exposes a public listener or DAW commands.
Requires openssl executable available on PATH.
"""
import shutil
import socket
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

from tls_client import verified_tls_client
from tls_loopback_server import serve_tls_once


class TLSRoundtripTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which("openssl"), "openssl CLI required")
    def test_encrypted_local_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            cert = Path(directory) / "cert.pem"
            key = Path(directory) / "key.pem"
            subprocess.run([
                "openssl", "req", "-x509", "-newkey", "rsa:2048",
                "-nodes", "-keyout", str(key), "-out", str(cert),
                "-days", "1", "-subj", "/CN=localhost",
                "-addext", "subjectAltName=DNS:localhost"
            ], check=True, capture_output=True, timeout=20)
            with socket.socket() as probe:
                probe.bind(("127.0.0.1", 0))
                port = probe.getsockname()[1]
            ready = threading.Event()
            failures = []

            def server():
                try:
                    serve_tls_once(port, str(cert), str(key), ready=ready)
                except Exception as exc:
                    failures.append(exc)

            worker = threading.Thread(target=server, daemon=True)
            worker.start()
            self.assertTrue(ready.wait(5), "localhost TLS server did not start")
            response = verified_tls_client(
                "127.0.0.1", port, "localhost", str(cert), b"tls-roundtrip"
            )
            worker.join(timeout=5)
            self.assertFalse(worker.is_alive())
            self.assertEqual(failures, [])
            self.assertEqual(response, b"tls-roundtrip")


if __name__ == "__main__":
    unittest.main()
