import unittest
from host_approval_ledger import HostApprovalLedger

class LedgerTests(unittest.TestCase):
    def test_single_use_and_scope(self):
        ledger=HostApprovalLedger()
        ledger.approve("guest",12,"req1")
        self.assertFalse(ledger.consume("other",12,"req1"))
        self.assertFalse(ledger.consume("guest",13,"req1"))
        self.assertFalse(ledger.consume("guest",12,"req2"))
        self.assertTrue(ledger.consume("guest",12,"req1"))
        self.assertFalse(ledger.consume("guest",12,"req1"))

    def test_revoke(self):
        ledger=HostApprovalLedger()
        ledger.approve("guest",12,"req1")
        ledger.revoke_peer("guest")
        self.assertFalse(ledger.consume("guest",12,"req1"))

if __name__=="__main__":
    unittest.main()
