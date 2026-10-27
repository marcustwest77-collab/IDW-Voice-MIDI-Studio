import ssl
import unittest
from unittest import mock

from tls_client import verified_tls_client


class TLSClientTest(unittest.TestCase):
    def test_missing_trust_or_server_name_fails_before_connect(self):
        for name, ca in (("", "ca.pem"), ("test.local", "")):
            with self.subTest(name=name, ca=ca):
                with self.assertRaises(ValueError):
                    verified_tls_client("127.0.0.1", 9001, name, ca, b"hello")

    def test_rejects_bad_ports_and_payloads(self):
        for port, data in ((0, b"hello"), (9001, b""), (9001, b"x" * 4097)):
            with self.assertRaises(ValueError):
                verified_tls_client("127.0.0.1", port, "test.local", "ca.pem", data)

    @mock.patch("tls_client.ssl.create_default_context")
    @mock.patch("tls_client.socket.create_connection")
    def test_tls_context_forces_verification(self, connection, create_context):
        tls = create_context.return_value.wrap_socket.return_value.__enter__.return_value
        tls.recv.side_effect = [bytes([0,0,0,2]), b"ok"]
        result = verified_tls_client("127.0.0.1", 9001, "test.local", "ca.pem", b"hi")
        self.assertEqual(result, b"ok")
        ctx = create_context.return_value
        self.assertEqual(ctx.verify_mode, ssl.CERT_REQUIRED)
        self.assertTrue(ctx.check_hostname)
        self.assertEqual(ctx.minimum_version, ssl.TLSVersion.TLSv1_2)
        create_context.return_value.wrap_socket.assert_called_once()
        self.assertEqual(create_context.return_value.wrap_socket.call_args.kwargs["server_hostname"], "test.local")


if __name__ == "__main__":
    unittest.main()
