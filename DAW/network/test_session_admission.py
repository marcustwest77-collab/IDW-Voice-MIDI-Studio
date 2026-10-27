import unittest
from session_admission import SessionAdmission


class SessionAdmissionTest(unittest.TestCase):
    def test_unverified_or_wrong_peer_cannot_join(self):
        session = SessionAdmission("guest-1", 42)
        self.assertFalse(session.admit("guest-2", True, True))
        self.assertFalse(session.admit("guest-1", False, True))
        self.assertFalse(session.admit("guest-1", True, False))
        self.assertFalse(session.grant(True, True))
        self.assertTrue(session.admit("guest-1", True, True))
        self.assertTrue(session.grant(True, False))
        self.assertTrue(session.can_request("guest-1", 42))
        self.assertFalse(session.can_request("guest-1", 42, recording=True))
        self.assertFalse(session.can_request("guest-2", 42))
        self.assertFalse(session.can_request("guest-1", 41))
        self.assertTrue(session.grant(True, True))
        self.assertTrue(session.can_request("guest-1", 42, recording=True))
        session.revoke()
        self.assertFalse(session.can_request("guest-1", 42))
        self.assertFalse(session.grant(True, True))

    def test_invalid_session(self):
        with self.assertRaises(ValueError):
            SessionAdmission("../bad", 1)
        with self.assertRaises(ValueError):
            SessionAdmission("guest", 0)


if __name__ == "__main__":
    unittest.main()
