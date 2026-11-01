"""Verified native decision -> live exact mailbox -> single-use host ledger."""
import os
import subprocess
import tempfile
import threading
import time
import unittest
from dataclasses import replace
from unittest.mock import patch

from native_command_approval import NativeCommandApprovalLedger
from native_tls_session import TLSConnectionIdentity
from native_tls_offer import NativeTLSOffer
from windows_native_command_review import review_windows_native_command
from loopback_transport import send_frame,receive_frame
from test_native_local_bridge import issue_certificate
from test_native_tls_session import SessionHarness

IDENTITY=TLSConnectionIdentity("guest",45,"a"*64,"b"*64,"TLSv1.3")
WIRE=b"1|45|1|adapter|arm|1"
SCOPE=("guest",45,"b"*64)


class ReviewAdapterTests(unittest.TestCase):
    def setUp(self):
        self.ledger=NativeCommandApprovalLedger();self.ledger.begin_session(*SCOPE)
        self.cancel=threading.Event()

    def review(self,**changes):
        options=dict(ledger=self.ledger,name="trusted endpoint",expected_server_handle=1,
            cancel=self.cancel,deadline=time.monotonic()+1)
        options.update(changes)
        return review_windows_native_command(IDENTITY,WIRE,**options)

    def tearDown(self):
        self.assertFalse(any(t.name=="idw-private-command-mailbox" for t in threading.enumerate()))

    def test_exact_native_allow_writes_one_approval_and_retains_lifetime_context(self):
        deadline=time.monotonic()+1
        with patch("windows_native_command_review.receive_windows_command_decision",return_value=True) as decision:
            self.assertTrue(self.review(deadline=deadline))
            prompt=decision.call_args.args[2]
            self.assertEqual((prompt.identity,prompt.payload),(IDENTITY,WIRE))
            self.assertIs(decision.call_args.kwargs['cancel'],self.cancel)
            self.assertEqual(decision.call_args.kwargs['deadline'],deadline)
            decision.assert_called_once()
        self.assertTrue(self.ledger.consume(*SCOPE,WIRE));self.assertFalse(self.ledger.consume(*SCOPE,WIRE))

    def test_native_denial_or_nonboolean_result_cannot_approve(self):
        for result in (False,1,None):
            with patch("windows_native_command_review.receive_windows_command_decision",return_value=result) as decision:
                self.assertFalse(self.review());decision.assert_called_once()
            self.assertFalse(self.ledger.consume(*SCOPE,WIRE))

    def test_invalid_scope_or_inactive_session_never_calls_pipe(self):
        with patch("windows_native_command_review.receive_windows_command_decision") as decision:
            with self.assertRaises(ValueError): self.review(deadline=float("nan"))
            self.assertFalse(self.review(deadline=time.monotonic()-1))
            self.cancel.set();self.assertFalse(self.review());self.cancel.clear()
            self.ledger.revoke_session(*SCOPE);self.assertFalse(self.review())
            decision.assert_not_called()

    def test_late_allow_after_cancel_expiry_or_revocation_cannot_approve(self):
        for mode in ("cancel","expire","revoke"):
            ledger=NativeCommandApprovalLedger();ledger.begin_session(*SCOPE);cancel=threading.Event()
            def decision(*args,**kwargs):
                if mode=="cancel": cancel.set()
                if mode=="expire": time.sleep(.08)
                if mode=="revoke": ledger.revoke_session(*SCOPE)
                return True
            with patch("windows_native_command_review.receive_windows_command_decision",side_effect=decision):
                self.assertFalse(self.review(ledger=ledger,cancel=cancel,deadline=time.monotonic()+(.05 if mode=="expire" else 1)))
            self.assertFalse(ledger.consume(*SCOPE,WIRE))

    def test_pipe_failure_closes_and_joins_mailbox_without_retry(self):
        with patch("windows_native_command_review.receive_windows_command_decision",side_effect=ConnectionError("pipe failed")) as decision:
            with self.assertRaisesRegex(ConnectionError,"pipe failed"): self.review()
            decision.assert_called_once()
        self.assertFalse(self.ledger.consume(*SCOPE,WIRE))

    def test_bad_endpoint_configuration_unwinds_worker_before_any_pipe_open(self):
        with patch("windows_native_command_review._VerifiedPipe") as opened:
            with self.assertRaises(ValueError): self.review()
            opened.assert_not_called()
        self.assertFalse(self.ledger.consume(*SCOPE,WIRE))

    def test_each_transaction_uses_a_fresh_mailbox_lifetime(self):
        with patch("windows_native_command_review.receive_windows_command_decision",return_value=True) as decision:
            self.assertTrue(self.review());self.ledger.consume(*SCOPE,WIRE)
            self.assertTrue(self.review())
            first,second=(call.args[2] for call in decision.call_args_list)
            self.assertNotEqual(first.review_owner,second.review_owner)

    def test_worker_start_failure_never_opens_pipe_or_approves(self):
        with patch("windows_native_command_review.threading.Thread.start",side_effect=RuntimeError("worker unavailable")), \
                patch("windows_native_command_review.receive_windows_command_decision") as decision:
            with self.assertRaisesRegex(RuntimeError,"worker unavailable"): self.review()
            decision.assert_not_called()
        self.assertFalse(self.ledger.consume(*SCOPE,WIRE))


@unittest.skipUnless(os.name=="nt" and os.environ.get("IDW_COMMAND_REVIEW_PIPE_FIXTURE") and
    os.environ.get("IDW_NATIVE_LOCAL_FIXTURE"),"Windows decision and queue fixtures required")
class ReviewAdapterTLSIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder=tempfile.TemporaryDirectory();cls.addClassCleanup(cls.folder.cleanup)
        cls.cert,cls.key=issue_certificate(cls.folder.name,"localhost")
        cls.guest,cls.guest_key=issue_certificate(cls.folder.name,"guest")

    def harness(self,mode="allow",**limits):
        hosts=[];entered=threading.Event()
        def factory(ledger):
            class Reviewer:
                def request(owner,identity,payload,*,cancel,deadline):
                    host=subprocess.Popen([os.environ["IDW_COMMAND_REVIEW_PIPE_FIXTURE"],"--test-bound-host",str(os.getpid()),mode],
                        stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0,
                        close_fds=True,creationflags=subprocess.CREATE_NO_WINDOW)
                    hosts.append(host)
                    binding=replace(identity,connection_id="d"*64) if mode=="mismatch" else identity
                    host.stdin.write(NativeTLSOffer(binding.epoch,binding.peer_sha256,binding.connection_id,binding.tls_version).encode())
                    host.stdin.flush();record=host.stdout.readline()
                    if not record.startswith(b"PIPE "): raise RuntimeError("native decision host not ready")
                    entered.set()
                    return review_windows_native_command(identity,payload,ledger=ledger,name=record[5:-1].decode("ascii"),
                        expected_server_handle=int(host._handle),cancel=cancel,deadline=deadline)
            return Reviewer()
        harness=SessionHarness(self.cert,self.key,self.guest,self.guest_key,owned_receiver=True,
            private_bootstrap=True,socket_revocation=True,command_review_factory=factory,**limits)
        def cleanup():
            harness.close()
            for host in hosts:
                if host.poll() is None: host.kill()
                host.communicate(timeout=3)
        self.addCleanup(cleanup)
        return harness,hosts,entered

    def test_real_tls_native_decision_pipe_and_native_queue_without_python_preapproval(self):
        harness,hosts,entered=self.harness("manual")
        client=harness.connect();self.assertTrue(harness.native_ready.wait(2));send_frame(client,WIRE)
        self.assertTrue(entered.wait(2));self.assertEqual(hosts[0].stdout.readline(),b"REVIEW\n")
        self.assertEqual(harness.fixture.pending(),0)
        identity=harness.identities[0]
        self.assertFalse(harness.ledger.consume(identity.peer_id,identity.epoch,identity.connection_id,WIRE))
        hosts[0].stdin.write(b"ALLOW\n");hosts[0].stdin.flush() # Explicit trusted TEST-controller decision.
        self.assertEqual(receive_frame(client),WIRE)
        self.assertTrue(entered.is_set());self.assertEqual(harness.fixture.pending(),1)
        hosts[0].wait(timeout=3);self.assertEqual(hosts[0].stdout.read(),b"DONE\n")
        identity=harness.identities[0]
        self.assertFalse(harness.ledger.consume(identity.peer_id,identity.epoch,identity.connection_id,WIRE))
        client.close();harness.join();self.assertEqual(harness.fixture.pending(),0);self.assertEqual(harness.errors,[])

    def test_native_denial_binding_mismatch_and_expiry_never_queue(self):
        for mode in ("deny","mismatch","expire"):
            harness,hosts,entered=self.harness(mode)
            client=harness.connect();self.assertTrue(harness.native_ready.wait(2));send_frame(client,WIRE)
            with self.subTest(mode=mode),self.assertRaises(OSError): receive_frame(client)
            harness.join();self.assertEqual(harness.fixture.pending(),0)
            identity=harness.identities[0]
            self.assertFalse(harness.ledger.is_active(identity.peer_id,identity.epoch,identity.connection_id))

    def test_session_cancellation_while_native_decision_waits_never_queues(self):
        harness,hosts,entered=self.harness("slow")
        client=harness.connect();self.assertTrue(harness.native_ready.wait(2));send_frame(client,WIRE)
        self.assertTrue(entered.wait(2));self.assertEqual(hosts[0].stdout.readline(),b"REVIEW\n")
        self.assertEqual(harness.fixture.pending(),0);harness.cancel_session()
        with self.assertRaises(OSError): receive_frame(client)
        harness.join();self.assertEqual(harness.fixture.pending(),0)
        self.assertFalse(any(t.name=="idw-private-command-mailbox" for t in threading.enumerate()))

    def test_native_allow_after_host_consent_revocation_cannot_queue(self):
        harness,hosts,entered=self.harness("manual")
        client=harness.connect();self.assertTrue(harness.native_ready.wait(2));send_frame(client,WIRE)
        self.assertTrue(entered.wait(2));self.assertEqual(hosts[0].stdout.readline(),b"REVIEW\n")
        self.assertEqual(harness.fixture.pending(),0);harness.allowed=False
        hosts[0].stdin.write(b"ALLOW\n");hosts[0].stdin.flush()
        with self.assertRaises(OSError): receive_frame(client)
        harness.join();self.assertEqual(harness.fixture.pending(),0)
        identity=harness.identities[0]
        self.assertFalse(harness.ledger.is_active(identity.peer_id,identity.epoch,identity.connection_id))


if __name__=="__main__": unittest.main()
