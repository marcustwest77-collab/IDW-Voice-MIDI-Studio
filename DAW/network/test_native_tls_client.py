"""Pinned loopback client policy, real mTLS, failure cleanup and shared budget."""
import hashlib
from pathlib import Path
import socket
import ssl
import shutil
import subprocess
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

from native_tls_client import build_native_client_context, connect_native_tls_client
from native_tls_policy import build_native_server_context
from test_native_local_bridge import issue_certificate


class NativeTLSClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory(prefix="idw-native-client-")
        cls.addClassCleanup(cls.folder.cleanup)
        cls.cert, cls.key = issue_certificate(cls.folder.name, "localhost")
        cls.client, cls.client_key = issue_certificate(cls.folder.name, "guest")
        cls.other, cls.other_key = issue_certificate(cls.folder.name, "other")
        cls.pin = hashlib.sha256(ssl.PEM_cert_to_DER_cert(cls.cert.read_text())).hexdigest()

    def options(self, **changes):
        options = dict(approved_server_sha256=self.pin, server_ca_pem=self.cert.read_text(),
                       certificate_file=self.client, private_key_file=self.client_key, timeout=1)
        options.update(changes);return options

    def test_explicit_context_never_loads_default_or_environment_roots(self):
        baseline = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT).get_ca_certs(binary_form=True)
        options = self.options();options.pop("approved_server_sha256");options.pop("timeout")
        with patch.object(ssl.SSLContext, "load_default_certs", side_effect=AssertionError("default roots")), \
                patch.object(ssl.SSLContext, "set_default_verify_paths", side_effect=AssertionError("environment roots")):
            context = build_native_client_context(**options)
        self.assertEqual(context.verify_mode, ssl.CERT_REQUIRED)
        self.assertTrue(context.check_hostname)
        self.assertEqual(context.minimum_version, ssl.TLSVersion.TLSv1_2)
        self.assertTrue(context.options & ssl.OP_NO_TICKET)
        self.assertEqual(set(context.get_ca_certs(binary_form=True)),
                         set(baseline) | {ssl.PEM_cert_to_DER_cert(self.cert.read_text())})

    def test_invalid_configuration_or_credentials_cannot_connect(self):
        cases = [dict(timeout=v) for v in (True, 0, 6, float("nan"), float("inf"), 10**1000)]
        cases += [dict(approved_server_sha256=v) for v in ("", "A"*64, b"a"*64)]
        cases += [dict(certificate_file="relative.crt"), dict(private_key_file="relative.key"),
                  dict(private_key_file=self.other_key), dict(certificate_file=Path(self.folder.name)/"missing"),
                  dict(server_ca_pem=""), dict(server_ca_pem=b"PEM"), dict(server_ca_pem="\0"),
                  dict(server_ca_pem="x"*(1024*1024+1)), dict(server_ca_pem="invalid PEM")]
        with patch("native_tls_client.socket.socket") as opened:
            for changes in cases:
                with self.subTest(fields=changes.keys()), self.assertRaises((ValueError, OSError)):
                    with connect_native_tls_client(12345, **self.options(**changes)): self.fail("connected")
            for port in (True, 0, 65536, "443"):
                with self.assertRaises(ValueError):
                    with connect_native_tls_client(port, **self.options()): self.fail("connected")
            opened.assert_not_called()

    def test_encrypted_client_key_rejects_without_prompt_or_connection(self):
        encrypted=Path(self.folder.name)/"encrypted-client.key"
        if shutil.which("openssl"):
            subprocess.run(["openssl","pkey","-in",str(self.client_key),"-out",str(encrypted),
                "-aes256","-passout","pass:test-only-password"],check=True,capture_output=True,timeout=10)
        else:
            from cryptography.hazmat.primitives import serialization
            key=serialization.load_pem_private_key(self.client_key.read_bytes(),password=None)
            encrypted.write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,
                serialization.BestAvailableEncryption(b"test-only-password")))
        with patch("native_tls_client.socket.socket") as opened:
            with self.assertRaisesRegex(ValueError,"encrypted TLS private keys"):
                with connect_native_tls_client(12345,**self.options(private_key_file=encrypted)): self.fail("connected")
            opened.assert_not_called()

    def test_tcp_or_tls_wrap_failure_closes_owned_raw_socket(self):
        for phase in ("connect","wrap"):
            with self.subTest(phase=phase), patch("native_tls_client.build_native_client_context") as context, \
                    patch("native_tls_client.socket.socket") as opened:
                raw=opened.return_value
                (raw.connect if phase=="connect" else context.return_value.wrap_socket).side_effect=OSError("test failure")
                with self.assertRaisesRegex(OSError,"test failure"):
                    with connect_native_tls_client(12345,**self.options()): self.fail("connected")
                raw.close.assert_called_once()

    def server(self, *, cert=None, key=None, stalled=False):
        context = build_native_server_context(certificate_file=cert or self.cert,
            private_key_file=key or self.key, client_ca_pem=self.client.read_text())
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.bind(("127.0.0.1", 0));listener.listen(1);listener.settimeout(2)
        port = listener.getsockname()[1];received=[];errors=[];stop=threading.Event()
        def serve():
            try:
                with listener:
                    raw, _ = listener.accept()
                    with raw:
                        raw.settimeout(2)
                        if stalled: stop.wait(2);return
                        with context.wrap_socket(raw, server_side=True) as secure:
                            received.append(secure.recv(1))
            except OSError as error: errors.append(error)
        worker=threading.Thread(target=serve);worker.start()
        def cleanup():
            stop.set();listener.close();worker.join(timeout=3)
            self.assertFalse(worker.is_alive())
        self.addCleanup(cleanup)
        return port, received, errors, worker

    def test_real_mtls_sends_no_application_data_and_closes_on_normal_exit(self):
        port, received, errors, worker = self.server()
        with connect_native_tls_client(port, **self.options()) as client:
            self.assertEqual(client.getpeername(), ("127.0.0.1", port))
            self.assertEqual(hashlib.sha256(client.getpeercert(binary_form=True)).hexdigest(), self.pin)
        self.assertEqual(client.fileno(), -1)
        worker.join(timeout=2);self.assertFalse(worker.is_alive())
        self.assertEqual(received, [b""]);self.assertEqual(errors, [])

    def test_caller_exception_closes_verified_socket(self):
        port, received, errors, worker = self.server()
        with self.assertRaisesRegex(RuntimeError, "caller failed"):
            with connect_native_tls_client(port, **self.options()) as client: raise RuntimeError("caller failed")
        self.assertEqual(client.fileno(), -1);worker.join(timeout=2)
        self.assertEqual(received, [b""]);self.assertEqual(errors, [])

    def test_wrong_pin_ca_or_hostname_never_yields_a_socket(self):
        for mode in ("pin", "ca", "hostname"):
            with self.subTest(mode=mode):
                port, received, errors, worker = self.server(
                    **(dict(cert=self.other,key=self.other_key) if mode=="hostname" else {}))
                changes = {"pin": dict(approved_server_sha256="0"*64), "ca": dict(server_ca_pem=self.other.read_text()),
                    "hostname": dict(server_ca_pem=self.other.read_text(), approved_server_sha256=
                        hashlib.sha256(ssl.PEM_cert_to_DER_cert(self.other.read_text())).hexdigest()),
                    "client": dict(certificate_file=self.other, private_key_file=self.other_key)}[mode]
                with self.assertRaises((PermissionError, ssl.SSLError)):
                    with connect_native_tls_client(port, **self.options(**changes)) as client:
                        self.fail("unverified server yielded")
                worker.join(timeout=2);self.assertFalse(worker.is_alive())
                self.assertTrue(not received or received==[b""])

    def test_server_rejects_untrusted_client_without_application_data(self):
        port, received, errors, worker = self.server()
        try:
            with connect_native_tls_client(port, **self.options(certificate_file=self.other,
                    private_key_file=self.other_key)) as client:
                # TLS 1.3 client-auth rejection can arrive after client handshake
                # completion, as an alert or EOF on the first application read.
                self.assertEqual(client.recv(1), b"")
        except ssl.SSLError:
            pass
        worker.join(timeout=2);self.assertFalse(worker.is_alive())
        self.assertEqual(received, []);self.assertEqual(len(errors),1)
        self.assertIsInstance(errors[0],ssl.SSLError)

    def test_stalled_handshake_uses_bounded_network_budget(self):
        port, _, _, _ = self.server(stalled=True)
        start=time.monotonic()
        with self.assertRaises(TimeoutError):
            with connect_native_tls_client(port, **self.options(timeout=.15)): self.fail("handshake completed")
        self.assertLess(time.monotonic()-start, 1)

    def test_tcp_time_cannot_renew_handshake_budget(self):
        # Controlled monotonic clock exposes the budget at the TCP/TLS boundary.
        with patch("native_tls_client.build_native_client_context") as context, \
                patch("native_tls_client.socket.socket") as opened, \
                patch("native_tls_client.time.monotonic", side_effect=[10,10.1,10.9,11.1]):
            with self.assertRaises(TimeoutError):
                with connect_native_tls_client(12345, **self.options()): self.fail("budget renewed")
            raw=opened.return_value;secure=context.return_value.wrap_socket.return_value
            self.assertAlmostEqual(raw.settimeout.call_args.args[0], .9)
            self.assertAlmostEqual(secure.settimeout.call_args.args[0], .1)
            secure.close.assert_called_once();raw.connect.assert_called_once_with(("127.0.0.1",12345))


if __name__ == "__main__": unittest.main()
