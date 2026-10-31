"""Explicit trust material is validated without opening a listener."""
from pathlib import Path
import shutil
import ssl
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from native_tls_policy import build_native_server_context
from test_native_local_bridge import issue_certificate


class NativeTLSPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = tempfile.TemporaryDirectory()
        cls.cert, cls.key = issue_certificate(cls.folder.name, "localhost")
        cls.client_ca, cls.other_key = issue_certificate(cls.folder.name, "guest")

    @classmethod
    def tearDownClass(cls): cls.folder.cleanup()

    def build(self, **changes):
        options = dict(certificate_file=self.cert, private_key_file=self.key,
                       client_ca_pem=self.client_ca.read_text())
        options.update(changes)
        return build_native_server_context(**options)

    def test_fresh_context_trusts_only_explicit_ca_and_requires_client_certificate(self):
        # The managed test runtime installs a distrusted root in each fresh
        # context. Preserve interpreter policy and verify we add only our CA.
        baseline = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER).get_ca_certs(binary_form=True)
        with patch("socket.socket") as opened, \
                patch.object(ssl.SSLContext, "load_default_certs", side_effect=AssertionError("no default trust")), \
                patch.object(ssl.SSLContext, "set_default_verify_paths", side_effect=AssertionError("no environment trust")):
            context = self.build()
            opened.assert_not_called()
        self.assertEqual(context.protocol, ssl.PROTOCOL_TLS_SERVER)
        self.assertEqual(context.minimum_version, ssl.TLSVersion.TLSv1_2)
        self.assertEqual(context.verify_mode, ssl.CERT_REQUIRED)
        expected = ssl.PEM_cert_to_DER_cert(self.client_ca.read_text())
        self.assertEqual(set(context.get_ca_certs(binary_form=True)), set(baseline) | {expected})
        self.assertTrue(context.options & ssl.OP_NO_TICKET)
        self.assertEqual(context.num_tickets, 0)
        self.assertIsNot(context, self.build())

    def test_invalid_policy_or_material_cannot_open_listener(self):
        cases = [dict(certificate_file="relative.crt"), dict(private_key_file="relative.key"),
                 dict(client_ca_pem=""), dict(client_ca_pem=b"PEM"),
                 dict(client_ca_pem="\0"), dict(client_ca_pem="x" * (1024 * 1024 + 1)),
                 dict(client_ca_pem="invalid PEM"), dict(private_key_file=self.other_key),
                 dict(certificate_file=Path(self.folder.name) / "missing.crt")]
        with patch("socket.socket") as opened:
            for changes in cases:
                with self.subTest(fields=tuple(changes)):
                    with self.assertRaises((ValueError, OSError)):
                        self.build(**changes)
            opened.assert_not_called()

    def test_encrypted_private_key_is_rejected_without_password_prompt(self):
        encrypted = Path(self.folder.name) / "encrypted.key"
        if shutil.which("openssl"):
            subprocess.run(["openssl", "pkey", "-in", str(self.key), "-out", str(encrypted),
                            "-aes256", "-passout", "pass:test-only-key-password"],
                           check=True, capture_output=True, timeout=10)
        else:
            from cryptography.hazmat.primitives import serialization
            key = serialization.load_pem_private_key(self.key.read_bytes(), password=None)
            encrypted.write_bytes(key.private_bytes(serialization.Encoding.PEM,
                serialization.PrivateFormat.PKCS8, serialization.BestAvailableEncryption(b"test-only-key-password")))
        with patch("socket.socket") as opened:
            with self.assertRaisesRegex(ValueError, "encrypted TLS private keys"):
                self.build(private_key_file=encrypted)
            opened.assert_not_called()


if __name__ == "__main__": unittest.main()
