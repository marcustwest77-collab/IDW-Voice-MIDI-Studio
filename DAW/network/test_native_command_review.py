"""Exact local decisions, cancellation and real TLS-to-native pending review."""
from dataclasses import replace
import os
import tempfile
import threading
import time
import unittest

from native_command_approval import NativeCommandApprovalLedger
from native_command_review import NativeCommandReview
from native_tls_session import TLSConnectionIdentity
from loopback_transport import send_frame, receive_frame
from test_native_local_bridge import issue_certificate
from test_native_tls_session import SessionHarness

IDENTITY=TLSConnectionIdentity("guest",45,"b"*64,"a"*64,"TLSv1.3")
WIRE=b"1|45|1|local_review|arm|1"


def wait_for(test):
    until=time.monotonic()+2
    while time.monotonic()<until:
        value=test()
        if value: return value
        time.sleep(.005)
    raise AssertionError("review state did not arrive")


class LocalReviewTests(unittest.TestCase):
    def setUp(self):
        self.reset()

    def reset(self):
        if hasattr(self,"review"): self.review.close()
        self.ledger=NativeCommandApprovalLedger();self.ledger.begin_session("guest",45,"a"*64)
        self.review=NativeCommandReview(self.ledger);self.cancel=threading.Event()
        self.addCleanup(self.review.close)

    def start(self, *, timeout=1, payload=WIRE):
        results=[];review=self.review
        worker=threading.Thread(target=lambda:results.append(review.request(IDENTITY,payload,
            cancel=self.cancel,deadline=time.monotonic()+timeout)))
        worker.start()
        def cleanup(): review.close();worker.join(timeout=2);self.assertFalse(worker.is_alive())
        self.addCleanup(cleanup)
        return worker,results

    def test_only_exact_local_snapshot_can_approve_once(self):
        worker,results=self.start();prompt=wait_for(self.review.pending)
        self.assertFalse(self.ledger.consume("guest",45,"a"*64,WIRE))
        for wrong in (replace(prompt,generation=prompt.generation+1),replace(prompt,payload=WIRE[:-1]+b"2"),
                replace(prompt,identity=replace(IDENTITY,connection_id="c"*64))):
            self.assertFalse(self.review.resolve(wrong,True))
        self.assertTrue(self.review.resolve(prompt,True));self.assertFalse(self.review.resolve(prompt,True))
        worker.join(timeout=2);self.assertEqual(results,[True]);self.assertIsNone(self.review.pending())
        self.assertTrue(self.ledger.consume("guest",45,"a"*64,WIRE))
        self.assertFalse(self.ledger.consume("guest",45,"a"*64,WIRE))

    def test_identical_replacement_request_rejects_old_generation(self):
        worker,results=self.start();old=wait_for(self.review.pending)
        self.review.resolve(old,False);worker.join(timeout=2);self.assertEqual(results,[False])
        worker,results=self.start();new=wait_for(self.review.pending)
        self.assertNotEqual(old.generation,new.generation);self.assertFalse(self.review.resolve(old,True))
        self.assertTrue(self.review.resolve(new,True));worker.join(timeout=2);self.assertEqual(results,[True])

    def test_replacement_mailbox_rejects_identical_old_prompt(self):
        worker,results=self.start();old=wait_for(self.review.pending)
        self.review.resolve(old,False);worker.join(timeout=2)
        self.review.close();self.review=NativeCommandReview(self.ledger)
        self.addCleanup(self.review.close)
        worker,results=self.start();new=wait_for(self.review.pending)
        self.assertEqual(old.generation,new.generation)
        self.assertNotEqual(old.review_owner,new.review_owner)
        self.assertFalse(self.review.resolve(old,True));self.assertTrue(self.review.resolve(new,True))
        worker.join(timeout=2);self.assertEqual(results,[True])

    def test_denial_expiry_cancel_close_and_ledger_revocation_never_approve(self):
        for mode in ("deny","expire","cancel","close","revoke"):
            with self.subTest(mode=mode):
                self.reset()
                worker,results=self.start(timeout=.15 if mode=="expire" else 1)
                prompt=wait_for(self.review.pending)
                if mode=="deny": self.review.resolve(prompt,False)
                if mode=="cancel": self.cancel.set()
                if mode=="close": self.review.close()
                if mode=="revoke": self.ledger.revoke_session("guest",45,"a"*64)
                worker.join(timeout=2);self.assertFalse(worker.is_alive());self.assertEqual(results,[False])
                self.assertIsNone(self.review.pending());self.assertFalse(self.review.resolve(prompt,True))
                self.assertFalse(self.ledger.consume("guest",45,"a"*64,WIRE))

    def test_concurrent_request_cannot_replace_displayed_proposal(self):
        worker,results=self.start();prompt=wait_for(self.review.pending)
        self.assertFalse(self.review.request(IDENTITY,b"1|45|2|other|arm|2",cancel=self.cancel,
            deadline=time.monotonic()+1))
        self.assertEqual(self.review.pending(),prompt)
        self.review.resolve(prompt,False);worker.join(timeout=2);self.assertEqual(results,[False])

    def test_invalid_or_expired_proposal_cannot_publish(self):
        for deadline in (True,float("nan"),float("inf"),10**1000,time.monotonic()+10):
            with self.assertRaises(ValueError): self.review.request(IDENTITY,WIRE,cancel=self.cancel,deadline=deadline)
        for payload in (bytearray(WIRE),b"1|045|1|local_review|arm|1",b"1|45|1|local_review|record|1"):
            with self.assertRaises(ValueError): self.review.request(IDENTITY,payload,cancel=self.cancel,deadline=time.monotonic()+1)
        self.assertFalse(self.review.request(IDENTITY,WIRE,cancel=self.cancel,deadline=time.monotonic()-1))
        self.assertIsNone(self.review.pending())


@unittest.skipUnless(os.environ.get("IDW_NATIVE_LOCAL_FIXTURE"),"native fixture required")
class LocalReviewTLSIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder=tempfile.TemporaryDirectory();cls.addClassCleanup(cls.folder.cleanup)
        cls.cert,cls.key=issue_certificate(cls.folder.name,"localhost")
        cls.guest_cert,cls.guest_key=issue_certificate(cls.folder.name,"guest")

    def harness(self, **options):
        return SessionHarness(self.cert,self.key,self.guest_cert,self.guest_key,
            owned_receiver=True,private_bootstrap=os.name=="nt",socket_revocation=True,
            command_review_factory=NativeCommandReview,**options)

    def test_explicit_local_decision_precedes_native_queue_and_eof_revokes(self):
        with self.harness() as harness:
            client=harness.connect();self.assertTrue(harness.native_ready.wait(2))
            send_frame(client,WIRE);prompt=wait_for(harness.command_review.pending)
            self.assertEqual(harness.fixture.pending(),0)
            self.assertEqual(prompt.payload,WIRE);self.assertEqual(prompt.identity,harness.identities[0])
            self.assertFalse(harness.command_review.resolve(replace(prompt,payload=WIRE[:-1]+b"2"),True))
            self.assertTrue(harness.command_review.resolve(prompt,True))
            self.assertEqual(receive_frame(client),WIRE);self.assertEqual(harness.fixture.pending(),1)
            client.close();harness.join();self.assertEqual(harness.fixture.pending(),0)
            self.assertFalse(harness.command_review.resolve(prompt,True));self.assertEqual(harness.errors,[])

    def test_denial_timeout_cancel_and_consent_revocation_do_not_queue(self):
        for mode in ("deny","expire","cancel","consent"):
            with self.subTest(mode=mode),self.harness(command_timeout=.3 if mode=="expire" else 2) as harness:
                client=harness.connect();self.assertTrue(harness.native_ready.wait(2))
                send_frame(client,WIRE);prompt=wait_for(harness.command_review.pending)
                self.assertEqual(harness.fixture.pending(),0)
                if mode=="deny": harness.command_review.resolve(prompt,False)
                if mode=="cancel": harness.cancel_session()
                if mode=="consent":
                    harness.allowed=False;harness.command_review.resolve(prompt,True)
                with self.assertRaises(OSError): receive_frame(client)
                harness.join();self.assertEqual(harness.fixture.pending(),0)
                self.assertIsNone(harness.command_review.pending())
                self.assertFalse(harness.ledger.is_active("guest",45,prompt.identity.connection_id))

    def test_callback_true_without_exact_ledger_approval_cannot_queue(self):
        with SessionHarness(self.cert,self.key,self.guest_cert,self.guest_key,owned_receiver=True,
                private_bootstrap=os.name=="nt",socket_revocation=True,
                host_command_review=lambda *args,**kwargs:True) as harness:
            client=harness.connect();self.assertTrue(harness.native_ready.wait(2))
            send_frame(client,WIRE)
            with self.assertRaises(OSError): receive_frame(client)
            harness.join();self.assertEqual(harness.fixture.pending(),0)
            self.assertTrue(any(isinstance(error,PermissionError) for error in harness.errors))


if __name__=="__main__": unittest.main()
