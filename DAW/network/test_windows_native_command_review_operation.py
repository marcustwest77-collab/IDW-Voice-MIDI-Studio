"""Owned native reviewer: synchronous process duplication and joined cancellation."""
import os
import subprocess
import sys
import threading
import time
import unittest

from test_windows_native_command_review import PROMPT
from windows_native_command_review import receive_windows_command_decision


@unittest.skipUnless(os.name=="nt" and os.environ.get("IDW_COMMAND_REVIEW_OPERATION_FIXTURE"),"Windows owned-operation fixture required")
class OwnedReviewOperationTests(unittest.TestCase):
    def host(self,mode="allow",pid=None):
        host=subprocess.Popen([os.environ["IDW_COMMAND_REVIEW_OPERATION_FIXTURE"],"--test-host",str(pid or os.getpid()),mode],
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0,close_fds=True,
            creationflags=subprocess.CREATE_NO_WINDOW)
        def cleanup():
            if host.poll() is None: host.kill()
            host.communicate(timeout=3)
        self.addCleanup(cleanup)
        line=host.stdout.readline()
        self.assertTrue(line.startswith(b"PIPE "),line)
        return host,line[5:-1].decode("ascii")

    def test_owned_duplicate_survives_source_close_and_explicit_allow_or_deny(self):
        for mode,allowed in (("allow",True),("deny",False)):
            host,name=self.host(mode)
            # C++ closes its borrowed process handle before exposing this name.
            self.assertIs(receive_windows_command_decision(name,int(host._handle),PROMPT),allowed)
            self.assertEqual(host.wait(timeout=3),0)
            self.assertEqual(host.stdout.read(),b"REVIEW\nDONE\n")

    def request(self,host,name):
        results=[];errors=[];cancel=threading.Event()
        def receive():
            try: results.append(receive_windows_command_decision(name,int(host._handle),PROMPT,cancel=cancel))
            except Exception as error: errors.append(error)
        worker=threading.Thread(target=receive);worker.start()
        def cleanup(): cancel.set();worker.join(timeout=3);self.assertFalse(worker.is_alive())
        self.addCleanup(cleanup)
        return worker,results,errors

    def test_controller_polling_and_manual_decision_do_not_block_pipe_worker(self):
        host,name=self.host("manual");worker,results,errors=self.request(host,name)
        self.assertEqual(host.stdout.readline(),b"REVIEW\n")
        self.assertTrue(worker.is_alive());self.assertEqual(results,[])
        host.stdin.write(b"ALLOW\n");host.stdin.flush();worker.join(timeout=3)
        self.assertFalse(worker.is_alive());self.assertEqual(results,[True]);self.assertEqual(errors,[])
        self.assertEqual(host.wait(timeout=3),0);self.assertEqual(host.stdout.read(),b"DONE\n")

    def test_stop_wakes_pending_mailbox_joins_and_returns_no_decision(self):
        host,name=self.host("manual");worker,results,errors=self.request(host,name)
        self.assertEqual(host.stdout.readline(),b"REVIEW\n")
        started=time.monotonic();host.stdin.write(b"STOP!\n");host.stdin.flush()
        worker.join(timeout=1);self.assertFalse(worker.is_alive())
        self.assertEqual(results,[]);self.assertTrue(errors);self.assertLess(time.monotonic()-started,1)
        self.assertEqual(host.wait(timeout=2),3);self.assertEqual(host.stdout.read(),b"FAILED\n")

    def test_idle_cancel_and_original_owner_deadline_cannot_publish_allow(self):
        for mode in ("cancel","expire"):
            host,name=self.host(mode)
            with self.subTest(mode=mode),self.assertRaises(ConnectionError):
                receive_windows_command_decision(name,int(host._handle),PROMPT)
            self.assertEqual(host.wait(timeout=3),3)

    def test_immediate_stop_cannot_be_undone_by_late_worker_entry(self):
        result=subprocess.run([os.environ["IDW_COMMAND_REVIEW_OPERATION_FIXTURE"],"--test-host",str(os.getpid()),"immediate"],
            input=b"",capture_output=True,timeout=2,creationflags=subprocess.CREATE_NO_WINDOW)
        self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout,b"STOPPED\n")

    def test_peer_exit_cancels_idle_exchange_using_owned_process_identity(self):
        peer=subprocess.Popen([sys.executable,"-c","import time;time.sleep(15)"],close_fds=True,
            creationflags=subprocess.CREATE_NO_WINDOW)
        self.addCleanup(lambda:(peer.kill(),peer.wait(timeout=3)) if peer.poll() is None else None)
        host,name=self.host(pid=peer.pid)
        peer.kill();peer.wait(timeout=3)
        self.assertEqual(host.wait(timeout=1.5),3);self.assertNotIn(b"REVIEW",host.stdout.read())


if __name__=="__main__": unittest.main()
