"""Cross-language protocol compatibility fixture, not a real TLS-to-DAW integration."""
import subprocess
import sys
import unittest

class CrossLanguageHandoff(unittest.TestCase):
    def test_serialized_commands(self):
        fixture=sys.argv[1]
        packets=[
            "1|45|1|first|arm|1",
            "1|45|1|replay|arm|1",
            "1|45|2|second|select|0",
            "1|45|3|recording|record|1",
            "1|46|4|wrong_epoch|arm|1",
            "1|45|4|third|arm|2",
            "TEST_AUTH_LOSS",
            "1|45|5|after_auth_loss|arm|1",
        ]
        result=subprocess.run([fixture],input="\n".join(packets)+"\n",
            text=True,capture_output=True,check=True,timeout=10)
        self.assertEqual(result.stdout.splitlines(),
            ["REVIEW","DENY","REVIEW","DENY","DENY","REVIEW","DISCONNECTED","DENY"])
        self.assertFalse(result.stderr)

if __name__=="__main__":
    unittest.main(argv=[sys.argv[0]])
