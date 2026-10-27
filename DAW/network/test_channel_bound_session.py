import unittest
from channel_bound_session import BoundSession


class BoundSessionTest(unittest.TestCase):
    def test_binding_required_for_access(self):
        s = BoundSession("guest-1", 70, "trustedchannelvalue_1234")
        self.assertFalse(s.admit_bound("guest-1", "anotherchannelvalue_123", True, True))
        self.assertTrue(s.admit_bound("guest-1", "trustedchannelvalue_1234", True, True))
        self.assertTrue(s.grant(True, True))
        self.assertTrue(s.can_request_bound("guest-1", 70, "trustedchannelvalue_1234", True))
        self.assertFalse(s.can_request_bound("guest-1", 70, "anotherchannelvalue_123", True))
        s.revoke()
        self.assertFalse(s.admit_bound("guest-1", "trustedchannelvalue_1234", True, True))
        self.assertFalse(s.can_request_bound("guest-1", 70, "trustedchannelvalue_1234", True))

    def test_invalid_binding(self):
        with self.assertRaises(ValueError):
            BoundSession("guest", 1, "short")


if __name__ == "__main__":
    unittest.main()
