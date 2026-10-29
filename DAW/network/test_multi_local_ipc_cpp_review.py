"""Development integration: token-authenticated local IPC -> persistent C++ review.

This runs only against the test fixture, not the JUCE application.
"""
import os
import socket
import threading
import unittest

from cpp_review_handoff import CppReviewHandoff
from local_command_bridge import LocalCommandBridge
from local_receiver_auth import LocalReceiverAuthenticator
from local_receiver_service import serve_local_session
from verified_event_producer import VerifiedCommandEvent


@unittest.skipUnless(os.environ.get("IDW_CPP_REVIEW_FIXTURE"), "C++ test fixture required")
class MultiCommandCppReviewTest(unittest.TestCase):
    def test_replay_does_not_terminate_persistent_cpp_review(self):
        token = LocalReceiverAuthenticator.fresh_secret()
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            port = probe.getsockname()[1]
        fixture = CppReviewHandoff(os.environ["IDW_CPP_REVIEW_FIXTURE"])
        self.addCleanup(fixture.close)
        ready = threading.Event()
        outcomes = []
        accepted = []
        def sink(wire):
            accepted.append(wire)
            return fixture.accept(wire)
        worker = threading.Thread(target=lambda: outcomes.append(
            serve_local_session(port,
                LocalReceiverAuthenticator(token, "guest", 45),
                sink, ready, max_connections=3)
        ))
        worker.start()
        self.assertTrue(ready.wait(5))
        bridge = LocalCommandBridge(port, session_token=token)
        self.assertTrue(bridge.deliver(VerifiedCommandEvent(
            45, "1|45|1|initial|arm|1", "guest")))
        self.assertFalse(bridge.deliver(VerifiedCommandEvent(
            45, "1|45|1|replay|arm|1", "guest")))
        self.assertTrue(bridge.deliver(VerifiedCommandEvent(
            45, "1|45|2|next|select|0", "guest")))
        worker.join(timeout=5)
        self.assertFalse(worker.is_alive())
        self.assertEqual(outcomes, [2])
        self.assertEqual(accepted, [
            "1|45|1|initial|arm|1", "1|45|2|next|select|0"
        ])


if __name__ == "__main__":
    unittest.main()
