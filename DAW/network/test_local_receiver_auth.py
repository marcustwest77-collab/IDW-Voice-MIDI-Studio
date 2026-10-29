import json
import unittest
from local_receiver_auth import LocalReceiverAuthenticator

class ReceiverAuthTests(unittest.TestCase):
 def test_authentication_and_replay(self):
  token=LocalReceiverAuthenticator.fresh_secret()
  receiver=LocalReceiverAuthenticator(token,"guest",45)
  body={"type":"command","epoch":45,"peer":"guest","wire":"1|45|2|r|arm|1","token":token}
  encode=lambda d: json.dumps(d).encode()
  self.assertIsNone(receiver.accept(encode({**body,"token":"x"*64})))
  self.assertIsNone(receiver.accept(encode({**body,"epoch":46})))
  self.assertIsNone(receiver.accept(encode({**body,"peer":"other"})))
  self.assertEqual(receiver.accept(encode(body)),body["wire"])
  self.assertIsNone(receiver.accept(encode(body)))
  self.assertEqual(receiver.accept(encode({**body,"wire":"1|45|3|r2|select|0"})),"1|45|3|r2|select|0")
if __name__=="__main__":
 unittest.main()
