import socket
import threading
import unittest
from local_receiver_auth import LocalReceiverAuthenticator
from local_receiver_service import serve_local_once
from local_command_bridge import LocalCommandBridge
from verified_event_producer import VerifiedEventProducer

class ReceiverServiceTests(unittest.TestCase):
 def run_exchange(self, client_token, server_token):
  with socket.socket() as probe:
   probe.bind(("127.0.0.1",0))
   port=probe.getsockname()[1]
  ready=threading.Event()
  accepted=[]
  outcomes=[]
  def run():
   outcomes.append(serve_local_once(
    port,LocalReceiverAuthenticator(server_token,"guest",45),
    lambda wire: accepted.append(wire) or True,ready))
  worker=threading.Thread(target=run)
  worker.start()
  self.assertTrue(ready.wait(5))
  producer=VerifiedEventProducer("guest",45,
      LocalCommandBridge(port,session_token=client_token).deliver)
  if client_token==server_token:
   producer.accepted_command_sink({"session_epoch":45,"sequence":2,
       "request_id":"approved","action":"select","track_index":1},"guest")
  else:
   with self.assertRaises(PermissionError):
    producer.accepted_command_sink({"session_epoch":45,"sequence":2,
        "request_id":"denied","action":"arm","track_index":1},"guest")
  worker.join(timeout=5)
  self.assertFalse(worker.is_alive())
  return outcomes, accepted

 def test_authenticated_receiver_delivers(self):
  token=LocalReceiverAuthenticator.fresh_secret()
  outcomes, accepted=self.run_exchange(token,token)
  self.assertEqual(outcomes,[True])
  self.assertEqual(accepted,["1|45|2|approved|select|1"])

 def test_wrong_secret_fails_closed(self):
  outcomes, accepted=self.run_exchange(
   LocalReceiverAuthenticator.fresh_secret(),
   LocalReceiverAuthenticator.fresh_secret())
  self.assertEqual(outcomes,[False])
  self.assertEqual(accepted,[])

if __name__=="__main__":
 unittest.main()
