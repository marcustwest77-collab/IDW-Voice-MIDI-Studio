"""Atomic protected-bootstrap/lease handoff and real Windows native teardown."""
from dataclasses import replace
import os
import threading
import unittest
from unittest.mock import MagicMock, patch

from native_local_bridge import NativeLocalBootstrap
from native_tls_session import TLSConnectionIdentity
from windows_native_lease import receive_windows_native_lease, NativeLeaseRollbackError
import test_native_tls_receiver as receivers
import test_native_tls_session as sessions

NAME = "\\\\.\\pipe\\IDW-LocalBootstrap-" + "a" * 64


class LeaseTransactionTests(unittest.TestCase):
    def setUp(self):
        self.identity = TLSConnectionIdentity("guest", 45, "a" * 64, "b" * 64, "TLSv1.3")
        self.bootstrap = NativeLocalBootstrap(12345, 45, "1" * 64, "a" * 64, "b" * 64)

    def receive(self, **options):
        return receive_windows_native_lease(NAME, 123, self.identity, **options)

    def test_invalid_identity_and_revocation_configuration_never_contact_pipe(self):
        with patch("windows_native_lease.receive_windows_bootstrap") as receive:
            for value in (None, replace(self.identity, epoch=True), replace(self.identity, peer_id=""),
                          replace(self.identity, connection_id="0"), replace(self.identity, tls_version="TLSv1")):
                with self.assertRaises(ValueError): receive_windows_native_lease(NAME, 123, value)
            for timeout in (True, 0, 6, float("nan"), 10**1000):
                with self.assertRaises(ValueError): self.receive(revoke_timeout=timeout)
            receive.assert_not_called()

    def test_lease_is_constructed_before_acknowledgment_and_transferred_once(self):
        lease = MagicMock()
        def receive(name, handle, **options):
            self.assertEqual((name, handle), (NAME, 123))
            self.assertTrue(options["consume"](self.bootstrap))
            factory.assert_called_once_with(self.bootstrap, self.identity, timeout=1.0)
            lease.close.assert_not_called()
            return True # Stand-in for native acknowledgment completion.
        with patch("windows_native_lease.native_socket_lease", return_value=lease) as factory, \
                patch("windows_native_lease.receive_windows_bootstrap", side_effect=receive):
            self.assertIs(self.receive(), lease)
        lease.close.assert_not_called() # Ownership has transferred to the session owner.

    def test_failed_acknowledgment_or_pipe_exception_rolls_back_constructed_lease(self):
        for fails in (False, True):
            lease = MagicMock()
            def receive(*args, **options):
                self.assertTrue(options["consume"](self.bootstrap))
                if fails: raise OSError("test pipe failure")
                return False
            with patch("windows_native_lease.native_socket_lease", return_value=lease), \
                    patch("windows_native_lease.receive_windows_bootstrap", side_effect=receive):
                with self.assertRaises(ConnectionError if not fails else OSError): self.receive()
            lease.close.assert_called_once()

    def test_cancellation_before_consume_and_after_acknowledgment(self):
        cancel = threading.Event();cancel.set()
        with patch("windows_native_lease.receive_windows_bootstrap") as receive:
            with self.assertRaises(ConnectionAbortedError): self.receive(cancel=cancel)
            receive.assert_not_called()
        for after_consume in (False, True):
            cancel.clear();lease = MagicMock()
            def receive(*args, **options):
                if not after_consume: cancel.set()
                consumed = options["consume"](self.bootstrap)
                cancel.set()
                return consumed
            with patch("windows_native_lease.native_socket_lease", return_value=lease) as factory, \
                    patch("windows_native_lease.receive_windows_bootstrap", side_effect=receive):
                with self.assertRaises(ConnectionError): self.receive(cancel=cancel)
            if after_consume: lease.close.assert_called_once()
            else: factory.assert_not_called();lease.close.assert_not_called()

    def test_binding_construction_failure_does_not_acknowledge(self):
        def receive(*args, **options):
            options["consume"](replace(self.bootstrap, epoch=46))
            self.fail("invalid bootstrap must not be acknowledged")
        with patch("windows_native_lease.receive_windows_bootstrap", side_effect=receive):
            with self.assertRaises(ValueError): self.receive()

    def test_rollback_failure_preserves_both_causes(self):
        original, rollback = OSError("test pipe failure"), ConnectionError("test revoke failure")
        lease = MagicMock();lease.close.side_effect = rollback
        def receive(*args, **options):
            options["consume"](self.bootstrap)
            raise original
        with patch("windows_native_lease.native_socket_lease", return_value=lease), \
                patch("windows_native_lease.receive_windows_bootstrap", side_effect=receive):
            with self.assertRaises(NativeLeaseRollbackError) as caught: self.receive()
        self.assertIs(caught.exception.provisioning_error, original)
        self.assertIs(caught.exception.rollback_error, rollback)
        lease.close.assert_called_once()

    @unittest.skipIf(os.name == "nt", "unsupported-platform check")
    def test_unsupported_platform_returns_no_lease(self):
        with self.assertRaises(ConnectionError): self.receive()


@unittest.skipUnless(os.name == "nt", "Windows private pipe integration")
class TransactionalWindowsTLSIntegrationTests(receivers.OwnedReceiverIntegrationTests):
    def harness(self, **options):
        return sessions.SessionHarness(self.cert, self.key, self.guest_cert, self.guest_key,
            owned_receiver=True, private_bootstrap=True, transactional_lease=True, **options)


if __name__ == "__main__": unittest.main()
