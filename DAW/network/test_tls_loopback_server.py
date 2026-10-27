"""Isolated integration tests: ephemeral localhost TLS only, no external network."""
import socket
import ssl
import tempfile
import threading
import unittest
from pathlib import Path
from unittest import mock

from tls_client import verified_tls_client
from tls_loopback_server import serve_tls_once


from channel_bound_session import BoundSession
from replay_guard import ReplayGuard


class TLSServerTest(unittest.TestCase):
    def test_unpinned_protected_session_rejected_before_read(self):
        from unittest.mock import MagicMock
        session = MagicMock()
        with mock.patch("tls_loopback_server.ssl.SSLContext") as ctx, \
             mock.patch("tls_loopback_server.socket.socket") as make_socket:
            secure = ctx.return_value.wrap_socket.return_value.__enter__.return_value
            with self.assertRaises(PermissionError):
                serve_tls_once(38941, "cert", "key", session=session,
                               verified_peer_id="guest", verified_channel_id="trusted",
                               replay_guard=ReplayGuard())
            secure.recv.assert_not_called()


    def test_gate_requires_trusted_context(self):
        session = BoundSession("guest", 7, "trusted_channel_identity_123")
        self.assertTrue(session.admit_bound("guest", "trusted_channel_identity_123", True, True))
        self.assertTrue(session.grant(True, False))
        guard = ReplayGuard()
        self.assertEqual(guard.inspect(session, b"1|7|1|request1|arm|1",
                                      "guest", "trusted_channel_identity_123")["sequence"], 1)
        with self.assertRaises(PermissionError):
            guard.inspect(session, b"1|7|1|request1|arm|1",
                          "guest", "trusted_channel_identity_123")

    def test_requires_credentials(self):
        with self.assertRaises(ValueError):
            serve_tls_once(0, "certificate", "key")

    def test_tls_server_restricts_to_loopback(self):
        with mock.patch("tls_loopback_server.ssl.SSLContext") as ctx, \
             mock.patch("tls_loopback_server.socket.socket") as make_socket:
            listener = make_socket.return_value.__enter__.return_value
            listener.accept.side_effect = TimeoutError("test stop after bind")
            with self.assertRaises(TimeoutError):
                serve_tls_once(38941, "certificate", "key")
            listener.bind.assert_called_once_with(("127.0.0.1", 38941))
            ctx.return_value.load_cert_chain.assert_called_once_with(
                certfile="certificate", keyfile="key"
            )
            self.assertEqual(ctx.return_value.minimum_version, ssl.TLSVersion.TLSv1_2)


if __name__ == "__main__":
    unittest.main()
