import unittest

from channel_bound_session import BoundSession
from guarded_command_intake import inspect_request


class GuardedIntakeTest(unittest.TestCase):
    def setUp(self):
        self.session = BoundSession("guest-1", 99, "verifiedchannel_token_123")
        self.assertTrue(self.session.admit_bound(
            "guest-1", "verifiedchannel_token_123", True, True
        ))
        self.assertTrue(self.session.grant(control=True, recording=False))

    def test_permitted_control(self):
        msg = b"1|99|1|req1|arm|2"
        self.assertEqual(inspect_request(
            self.session, msg, "guest-1", "verifiedchannel_token_123"
        )["track_index"], 2)

    def test_denies_wrong_peer_channel_epoch_and_recording(self):
        cases = [
            (b"1|99|1|req1|arm|2", "other", "verifiedchannel_token_123"),
            (b"1|99|1|req1|arm|2", "guest-1", "otherchannel_token_1234"),
            (b"1|98|1|req1|arm|2", "guest-1", "verifiedchannel_token_123"),
            (b"1|99|1|req1|record|2", "guest-1", "verifiedchannel_token_123"),
        ]
        for msg, peer, channel in cases:
            with self.subTest(msg=msg, peer=peer):
                with self.assertRaises(PermissionError):
                    inspect_request(self.session, msg, peer, channel)

    def test_rejects_destructive_or_malformed_command(self):
        with self.assertRaises(ValueError):
            inspect_request(self.session, b"1|99|1|req1|delete|2",
                            "guest-1", "verifiedchannel_token_123")

    def test_revoked_session_denied(self):
        self.session.revoke()
        with self.assertRaises(PermissionError):
            inspect_request(self.session, b"1|99|1|req1|arm|2",
                            "guest-1", "verifiedchannel_token_123")


if __name__ == "__main__":
    unittest.main()
