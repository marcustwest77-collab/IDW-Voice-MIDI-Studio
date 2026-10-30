"""TLS lifetime -> authenticated native termination; no test-stdio revoke."""
from dataclasses import replace
import os
import unittest
from unittest.mock import MagicMock, patch

from native_local_bridge import NativeLocalBootstrap
from native_tls_session import TLSConnectionIdentity, native_socket_lease
from test_native_local_bridge import NativeFixture, binding
import test_native_tls_receiver as receivers
import test_native_tls_session as sessions
from verified_event_producer import VerifiedCommandEvent


class RevocationTests(unittest.TestCase):
    def setUp(self):
        self.bootstrap = NativeLocalBootstrap(12345, 45, "1" * 64, "a" * 64, "b" * 64)
        self.identity = TLSConnectionIdentity("guest", 45, "a" * 64, "b" * 64, "TLSv1.3")

    def test_record_and_credential_retirement_on_success_and_failure(self):
        for reply in (b"1", b"0", b""):
            bridge = binding(self.bootstrap)
            connection = MagicMock();connection.__enter__.return_value = connection
            connection.recv.return_value = reply
            with patch("native_local_bridge.socket.create_connection", return_value=connection) as connect:
                self.assertEqual(bridge.revoke(), reply == b"1")
                packet = connection.sendall.call_args.args[0]
                self.assertEqual(packet[4:], b"1" * 64 + b"\nIDW_REVOKE|1|45")
                self.assertEqual(int.from_bytes(packet[:4], "big"), len(packet) - 4)
                self.assertFalse(bridge.revoke())
                self.assertFalse(bridge.deliver(VerifiedCommandEvent(45, "1|45|1|late|arm|1", "guest")))
                self.assertEqual(connect.call_count, 1)

    def test_deadline_failure_retires_credentials_without_connect(self):
        for deadline in (-1, float("inf"), True):
            bridge = binding(self.bootstrap)
            with patch("native_local_bridge.socket.create_connection") as connect:
                self.assertFalse(bridge.revoke(deadline=deadline));self.assertFalse(bridge.revoke())
                connect.assert_not_called()

    def test_lifecycle_record_is_not_a_deliverable_remote_action(self):
        with patch("native_local_bridge.socket.create_connection") as connect:
            self.assertFalse(binding(self.bootstrap).deliver(VerifiedCommandEvent(45, "IDW_REVOKE|1|45", "guest")))
            connect.assert_not_called()

    def test_native_lease_binding_and_cached_revocation_failure(self):
        for change in (dict(epoch=46), dict(epoch=True), dict(peer_sha256="0" * 64),
                       dict(connection_id="0" * 64)):
            with self.assertRaises(ValueError): native_socket_lease(self.bootstrap, replace(self.identity, **change))
        with patch("native_tls_session.NativeLocalCommandBridge") as bridge:
            bridge.return_value.revoke.return_value = False
            lease = native_socket_lease(self.bootstrap, self.identity)
            for _ in range(2):
                with self.assertRaises(ConnectionError): lease.close()
            bridge.return_value.revoke.assert_called_once()

    @unittest.skipUnless(os.environ.get("IDW_NATIVE_LOCAL_FIXTURE"), "native fixture required")
    def test_wrong_secret_does_not_revoke_and_success_closes_native_listener(self):
        fixture = NativeFixture("a" * 64, "b" * 64, private_bootstrap=os.name == "nt")
        self.addCleanup(fixture.close)
        bridge = binding(fixture.bootstrap)
        self.assertTrue(bridge.deliver(VerifiedCommandEvent(45, "1|45|1|first|arm|1", "guest")))
        wrong = binding(replace(fixture.bootstrap, _secret="0" * 64))
        self.assertFalse(wrong.revoke());self.assertEqual(fixture.pending(), 1)
        lease = native_socket_lease(fixture.bootstrap, self.identity)
        lease.close();lease.close()
        self.assertEqual(fixture.pending(), 0)
        self.assertFalse(bridge.deliver(VerifiedCommandEvent(45, "1|45|2|late|select|0", "guest")))
        bridge.close()


class SocketRevocationIntegrationTests(receivers.OwnedReceiverIntegrationTests):
    def harness(self, **options):
        return sessions.SessionHarness(self.cert, self.key, self.guest_cert, self.guest_key,
            owned_receiver=True, private_bootstrap=os.name == "nt", socket_revocation=True, **options)


if __name__ == "__main__": unittest.main()
