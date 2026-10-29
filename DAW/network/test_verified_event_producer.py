import unittest
from verified_event_producer import VerifiedEventProducer

class VerifiedProducerTests(unittest.TestCase):
    def test_accepted_command_delivery(self):
        events=[]
        producer=VerifiedEventProducer("guest",45,lambda event: events.append(event) or True)
        producer.accepted_command_sink(
            {"session_epoch":45,"sequence":2,"request_id":"arm_one",
             "action":"arm","track_index":1},"guest")
        self.assertEqual(len(events),1)
        self.assertEqual(events[0].wire,"1|45|2|arm_one|arm|1")
        self.assertEqual(events[0].peer_id,"guest")

    def test_denials_do_not_emit_events(self):
        events=[]
        producer=VerifiedEventProducer("guest",45,lambda event: events.append(event) or True)
        good={"session_epoch":45,"sequence":2,"request_id":"req",
              "action":"select","track_index":1}
        cases=[
            ({**good},"intruder"),
            ({**good,"session_epoch":46},"guest"),
            ({**good,"action":"record"},"guest"),
            ({**good,"sequence":0},"guest"),
            ({**good,"request_id":"unsafe|field"},"guest"),
            ({**good,"track_index":999},"guest"),
        ]
        for payload,peer in cases:
            with self.assertRaises(PermissionError):
                producer.accepted_command_sink(payload,peer)
        self.assertFalse(events)

    def test_failed_delivery_fails_closed(self):
        producer=VerifiedEventProducer("guest",45,lambda _:False)
        with self.assertRaises(PermissionError):
            producer.accepted_command_sink(
                {"session_epoch":45,"sequence":1,"request_id":"r",
                 "action":"arm","track_index":0},"guest")

if __name__=="__main__":
    unittest.main()
