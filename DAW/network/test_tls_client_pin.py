import hashlib
import shutil
import socket
import ssl
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

from tls_client import verified_tls_client
from tls_loopback_server import serve_tls_once


@unittest.skipUnless(shutil.which("openssl"), "OpenSSL CLI required")
class ClientCertificatePinTest(unittest.TestCase):
    def test_pinned_client_certificate(self):
        with tempfile.TemporaryDirectory() as folder:
            def generate(name):
                cert = Path(folder) / (name + ".crt")
                key = Path(folder) / (name + ".key")
                subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048",
                                "-nodes", "-keyout", str(key), "-out", str(cert),
                                "-days", "1", "-subj", "/CN=" + name,
                                "-addext", "subjectAltName=DNS:" + name],
                               check=True, capture_output=True, timeout=25)
                der = ssl.PEM_cert_to_DER_cert(cert.read_text())
                return cert, key, hashlib.sha256(der).hexdigest()
            srv_cert, srv_key, _ = generate("localhost")
            client_cert, client_key, fingerprint = generate("approved")
            def attempt(pin):
                with socket.socket() as probe:
                    probe.bind(("127.0.0.1", 0))
                    port = probe.getsockname()[1]
                ready = threading.Event()
                errors = []
                def server():
                    try:
                        serve_tls_once(port, str(srv_cert), str(srv_key), ready=ready,
                                       client_cafile=str(client_cert),
                                       approved_client_sha256=pin)
                    except Exception as exc:
                        errors.append(exc)
                worker = threading.Thread(target=server, daemon=True)
                worker.start()
                self.assertTrue(ready.wait(5))
                try:
                    response = verified_tls_client(
                        "127.0.0.1", port, "localhost", str(srv_cert),
                        b"client-proof", client_certfile=str(client_cert),
                        client_keyfile=str(client_key))
                except (ConnectionError, ssl.SSLError, OSError):
                    response = None
                worker.join(timeout=5)
                self.assertFalse(worker.is_alive())
                return response, errors
            self.assertEqual(attempt(fingerprint), (b"client-proof", []))
            response, errors = attempt("0" * 64 if fingerprint != "0" * 64 else "1" * 64)
            self.assertIsNone(response)
            self.assertTrue(any(isinstance(e, PermissionError) for e in errors))

    def test_pin_requires_certificate_trust(self):
        with self.assertRaises(ValueError):
            serve_tls_once(38941, "cert.pem", "key.pem",
                           approved_client_sha256="0" * 64)


if __name__ == "__main__":
    unittest.main()
