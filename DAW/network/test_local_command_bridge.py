import json
import socket
import threading
import unittest
from local_command_bridge import LocalCommandBridge
from verified_event_producer import VerifiedCommandEvent, VerifiedEventProducer

class LocalBridgeTests(unittest.TestCase):
    def test_loopback_handoff_and_ack(self):
        listener=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        listener.bind(("127.0.0.1",0))
        listener.listen(1)
        port=listener.getsockname()[1]
        received=[]
        def receiver():
            with listener:
                connection,_=listener.accept()
                with connection:
                    connection.settimeout(3)
                    header=connection.recv(4)
                    length=int.from_bytes(header,"big")
                    payload=b""
                    while len(payload)<length:
                        payload+=connection.recv(length-len(payload))
                    received.append(json.loads(payload))
                    connection.sendall(b"1")
        worker=threading.Thread(target=receiver)
        worker.start()
        producer=VerifiedEventProducer("guest",45,LocalCommandBridge(port).deliver)
        producer.accepted_command_sink(
            {"session_epoch":45,"sequence":2,"request_id":"go",
             "action":"select","track_index":1},"guest")
        worker.join(timeout=5)
        self.assertFalse(worker.is_alive())
        self.assertEqual(received[0],{
            "type":"command","epoch":45,"peer":"guest","wire":"1|45|2|go|select|1"})

    def test_unavailable_bridge_fails_closed(self):
        listener=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        listener.bind(("127.0.0.1",0))
        port=listener.getsockname()[1]
        listener.close()
        producer=VerifiedEventProducer("guest",45,LocalCommandBridge(port,timeout=0.3).deliver)
        with self.assertRaises(PermissionError):
            producer.accepted_command_sink(
                {"session_epoch":45,"sequence":1,"request_id":"req",
                 "action":"arm","track_index":1},"guest")

if __name__=="__main__":
    unittest.main()
