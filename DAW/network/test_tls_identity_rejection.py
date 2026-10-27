"""Negative identity verification tests for loopback TLS prototype."""
import shutil
import socket
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

from tls_client import verified_tls_client
from tls_loopback_server import serve_tls_once


@unittest.skipUnless(shutil.which("openssl"), "OpenSSL executable required")
class TLSIdentityRejectionTest(unittest.TestCase):
    def test_untrusted_certificate_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            cert = Path(directory) / "cert.pem"
            key = Path(directory) / "key.pem"
            unrelated = Path(directory) / "unrelated.pem"
            unrelated_key = Path(directory) / "unrelated.key"
            for output_cert, output_key in ((cert, key), (unrelated, unrelated_key)):
                subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048",
                                "-nodes", "-keyout", str(output_key), "-out", str(output_cert),
                                "-days", "1", "-subj", "/CN=localhost",
                                "-addext", "subjectAltName=DNS:localhost"],
                               check=True, capture_output=True, timeout=20)
            with socket.socket() as probe:
                probe.bind(("127.0.0.1", 0))
                port = probe.getsockname()[1]
            ready = threading.Event()
            def run_server():
                try:
                    serve_tls_once(port, str(cert), str(key), ready=ready)
                except Exception:
                    pass  # expected: handshake fails when client rejects certificate
            thread = threading.Thread(target=run_server, daemon=True)
            thread.start()
            self.assertTrue(ready.wait(5))
            import ssl
            with self.assertRaises(ssl.SSLCertVerificationError):
                verified_tls_client("127.0.0.1", port, "localhost",
                                    str(unrelated), b"not-authorized")
            thread.join(timeout=5)
            self.assertFalse(thread.is_alive())


if __name__ == "__main__":
    unittest.main()
