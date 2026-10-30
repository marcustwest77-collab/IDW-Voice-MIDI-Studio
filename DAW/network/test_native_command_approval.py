"""Exact host approval, concurrent single-use consumption and live TLS rejection."""
import os
import threading
import unittest

from native_command_approval import NativeCommandApprovalLedger
from loopback_transport import send_frame, receive_frame
import test_native_tls_receiver as receivers
import test_native_tls_session as sessions

SCOPE = ("guest", 45, "a" * 64)
WIRE = b"1|45|1|review|arm|1"


class ApprovalLedgerTests(unittest.TestCase):
    def setUp(self):
        self.ledger = NativeCommandApprovalLedger()
        self.assertTrue(self.ledger.begin_session(*SCOPE))

    def test_approval_matches_payload_and_connection_exactly(self):
        self.assertTrue(self.ledger.approve(*SCOPE, WIRE))
        for payload in (b"1|45|1|review|select|1", b"1|45|1|review|arm|2", b"1|45|2|review|arm|1"):
            self.assertFalse(self.ledger.consume(*SCOPE, payload))
            self.assertFalse(self.ledger.approve(*SCOPE, payload)) # No overwrite.
        self.assertFalse(self.ledger.consume("other", 45, "a" * 64, WIRE))
        self.assertFalse(self.ledger.consume("guest", 45, "b" * 64, WIRE))
        self.assertTrue(self.ledger.consume(*SCOPE, WIRE))
        self.assertFalse(self.ledger.consume(*SCOPE, WIRE))

    def test_bounds_and_invalid_commands(self):
        for scope in (("", 45, "a" * 64), ("guest", True, "a" * 64), ("guest", 45, "bad")):
            with self.assertRaises(ValueError): self.ledger.approve(*scope, WIRE)
        for payload in (bytearray(WIRE), b"1|045|1|review|arm|1", b"1|45|1|review|record|1",
                        b"1|45|1|review|arm|128", b"1|46|1|review|arm|1"):
            with self.assertRaises(ValueError): self.ledger.approve(*SCOPE, payload)
        for sequence in range(1, 33):
            self.assertTrue(self.ledger.approve(*SCOPE, f"1|45|{sequence}|r{sequence}|arm|1".encode()))
        self.assertFalse(self.ledger.approve(*SCOPE, b"1|45|33|extra|arm|1"))

    def test_revoke_clears_and_closes_against_late_approval(self):
        self.assertTrue(self.ledger.approve(*SCOPE, WIRE))
        self.assertTrue(self.ledger.revoke_session(*SCOPE))
        self.assertFalse(self.ledger.approve(*SCOPE, WIRE))
        self.assertFalse(self.ledger.consume(*SCOPE, WIRE))
        self.assertFalse(self.ledger.begin_session("guest", 45, "b" * 64))
        newer = ("guest", 46, "b" * 64)
        self.assertTrue(self.ledger.begin_session(*newer))
        self.assertFalse(self.ledger.revoke_session(*SCOPE))
        self.assertTrue(self.ledger.approve(*newer, b"1|46|1|new|arm|1"))

    def test_concurrent_consumers_have_one_winner(self):
        self.assertTrue(self.ledger.approve(*SCOPE, WIRE))
        barrier = threading.Barrier(9);results = []
        def consume():
            barrier.wait(timeout=2)
            results.append(self.ledger.consume(*SCOPE, WIRE))
        workers = [threading.Thread(target=consume) for _ in range(8)]
        for worker in workers: worker.start()
        barrier.wait(timeout=2)
        for worker in workers: worker.join(timeout=2);self.assertFalse(worker.is_alive())
        self.assertEqual(sum(results), 1)

    def test_active_ledger_cannot_be_rebound_by_another_session(self):
        self.assertTrue(self.ledger.approve(*SCOPE, WIRE))
        self.assertFalse(self.ledger.begin_session("guest", 46, "b" * 64))
        self.assertFalse(self.ledger.revoke_session("guest", 46, "b" * 64))
        self.assertTrue(self.ledger.consume(*SCOPE, WIRE))


class ExactApprovalTLSIntegrationTests(receivers.OwnedReceiverIntegrationTests):
    def harness(self, **options):
        return sessions.SessionHarness(self.cert, self.key, self.guest_cert, self.guest_key,
            owned_receiver=True, private_bootstrap=os.name == "nt", socket_revocation=True, **options)

    def test_changed_action_track_or_sequence_is_rejected_before_native_review(self):
        for wire in (b"1|45|1|review|select|1", b"1|45|1|review|arm|2", b"1|45|2|review|arm|1"):
            with self.subTest(wire=wire), self.harness() as harness:
                harness.connect();self.ready(harness)
                identity = harness.identities[0]
                self.assertTrue(harness.ledger.approve(identity.peer_id, 45, identity.connection_id, WIRE))
                send_frame(harness.client_socket, wire)
                with self.assertRaises(OSError): receive_frame(harness.client_socket)
                self.revoked(harness)
                self.assertEqual(harness.fixture.pending(), 0)
                self.assertTrue(any(isinstance(error, PermissionError) for error in harness.errors))
                self.assertFalse(harness.ledger.approve(identity.peer_id, 45, identity.connection_id, WIRE))

    def test_native_revocation_hook_failure_still_clears_python_approvals(self):
        # Deterministic owner callback failure, not a claim of a physical IPC fault.
        with self.harness(revoke_failure=True) as harness:
            harness.connect();self.ready(harness)
            identity = harness.identities[0]
            self.assertTrue(harness.ledger.approve(identity.peer_id, 45, identity.connection_id, WIRE))
            harness.cancel_session();harness.join()
            self.assertTrue(any(isinstance(error, RuntimeError) for error in harness.errors))
            self.assertEqual(harness.revocations, 0) # Native revocation was not confirmed.
            self.assertFalse(harness.ledger.consume(identity.peer_id, 45, identity.connection_id, WIRE))


if __name__ == "__main__": unittest.main()
