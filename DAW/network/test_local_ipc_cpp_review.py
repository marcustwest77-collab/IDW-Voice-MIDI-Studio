import os
import socket
import threading
import unittest
from local_receiver_auth import LocalReceiverAuthenticator
from local_receiver_service import serve_local_once
from local_command_bridge import LocalCommandBridge
from verified_event_producer import VerifiedEventProducer
from cpp_review_handoff import CppReviewHandoff

@unittest.skipUnless(os.environ.get("IDW_CPP_REVIEW_FIXTURE"),"C++ fixture needed")
class EndToEndLocalFixtureTest(unittest.TestCase):
 def test_authenticated_ipc_to_cpp_review(self):
  fixture=CppReviewHandoff(os.environ["IDW_CPP_REVIEW_FIXTURE"])
  self.addCleanup(fixture.close)
  secret=LocalReceiverAuthenticator.fresh_secret()
  with socket.socket() as probe:
   probe.bind(("127.0.0.1",0))
   port=probe.getsockname()[1]
  ready=threading.Event()
  outcome=[]
  worker=threading.Thread(target=lambda:outcome.append(serve_local_once(
   port,LocalReceiverAuthenticator(secret,"guest",45),fixture.accept,ready)))
  worker.start()
  self.assertTrue(ready.wait(5))
  producer=VerifiedEventProducer("guest",45,
      LocalCommandBridge(port,session_token=secret).deliver)
  producer.accepted_command_sink(
      {"session_epoch":45,"sequence":1,"request_id":"ipc_cpp",
       "action":"arm","track_index":1},"guest")
  worker.join(timeout=5)
  self.assertFalse(worker.is_alive())
  self.assertEqual(outcome,[True])

if __name__=="__main__":
 unittest.main()
