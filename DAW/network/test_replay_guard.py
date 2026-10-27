import unittest
from channel_bound_session import BoundSession
from replay_guard import ReplayGuard


class ReplayGuardTest(unittest.TestCase):
    def setUp(self):
        self.s = BoundSession("guest", 11, "trusted_channel_identity_123")
        self.assertTrue(self.s.admit_bound("guest", "trusted_channel_identity_123", True, True))
        self.assertTrue(self.s.grant(True, False))
        self.guard = ReplayGuard()

    def check(self, payload, peer="guest"):
        return self.guard.inspect(self.s, payload, peer, "trusted_channel_identity_123")

    def test_replay_and_reordered_commands(self):
        self.check(b"1|11|1|first|arm|1")
        for payload in [b"1|11|1|first|arm|1", b"1|11|1|different|arm|1",
                        b"1|11|2|first|arm|1"]:
            with self.assertRaises(PermissionError):
                self.check(payload)
        self.check(b"1|11|2|second|arm|1")

    def test_denial_does_not_advance_sequence(self):
        with self.assertRaises(PermissionError):
            self.check(b"1|11|8|denied|arm|1", peer="intruder")
        self.check(b"1|11|1|accepted|arm|1")
        self.s.revoke()
        with self.assertRaises(PermissionError):
            self.check(b"1|11|2|after_revoke|arm|1")


if __name__ == "__main__":
    unittest.main()
