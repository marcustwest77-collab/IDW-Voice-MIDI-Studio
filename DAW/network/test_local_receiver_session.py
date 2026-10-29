import socket
import threading
import unittest
from local_receiver_auth import LocalReceiverAuthenticator
from local_receiver_service import serve_local_session
from local_command_bridge import LocalCommandBridge
from verified_event_producer import VerifiedCommandEvent

class MultiCommandReceiverTests(unittest.TestCase):
 def test_replay_state_persists_across_connections(self):
  token=LocalReceiverAuthenticator.fresh_secret()
  with socket.socket() as probe:
   probe.bind(("127.0.0.1",0))
   port=probe.getsockname()[1]
  ready=threading.Event()
  accepted=[]
  count=[]
  worker=threading.Thread(target=lambda:count.append(serve_local_session(
      port,LocalReceiverAuthenticator(token,"guest",45),
      lambda wire: accepted.append(wire) or True,ready,max_connections=3)))
  worker.start()
  self.assertTrue(ready.wait(5))
  bridge=LocalCommandBridge(port,session_token=token)
  self.assertTrue(bridge.deliver(VerifiedCommandEvent(45,"1|45|1|first|select|0","guest")))
  self.assertFalse(bridge.deliver(VerifiedCommandEvent(45,"1|45|1|replay|select|0","guest")))
  self.assertTrue(bridge.deliver(VerifiedCommandEvent(45,"1|45|2|second|arm|1","guest")))
  worker.join(timeout=5)
  self.assertFalse(worker.is_alive())
  self.assertEqual(count,[2])
  self.assertEqual(accepted,["1|45|1|first|select|0","1|45|2|second|arm|1"])

if __name__=="__main__":
 unittest.main()
