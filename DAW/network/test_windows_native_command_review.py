"""Held-process-verified native command review pipe; synthetic TLS evidence."""
from dataclasses import replace
import os
import subprocess
import sys
import threading
import time
import unittest
from unittest.mock import patch, Mock

import windows_native_bootstrap as ipc
from windows_native_command_review import receive_windows_command_decision
from native_command_review import NativeCommandPrompt
from native_tls_session import TLSConnectionIdentity

PROMPT=NativeCommandPrompt(7,TLSConnectionIdentity("guest",45,"a"*64,"b"*64,"TLSv1.3"),
    b"1|45|1|review|arm|1","c"*32)
NAME="\\\\.\\pipe\\IDW-LocalCommandReview-"+"a"*64


class ReviewPipeConfigurationTests(unittest.TestCase):
    def test_response_size_and_binding_reject_before_return_and_close_scope(self):
        for oversized in (True,False):
            connection=Mock();reads=[]
            def transfer(data,writing):
                if not writing:
                    reads.append(len(data))
                    data[:]=(b"\xff\xff" if oversized else (198).to_bytes(2,"big")) if len(data)==2 else b"x"*len(data)
                return True
            connection.transfer.side_effect=transfer
            with patch("windows_native_command_review._VerifiedPipe",return_value=connection):
                with self.assertRaises(ValueError): receive_windows_command_decision(NAME,1,PROMPT)
            self.assertEqual(reads,[2] if oversized else [2,198])
            connection.close.assert_called_once()

    def test_invalid_configuration_never_opens_verified_pipe(self):
        cases=[dict(name="\\\\other\\pipe\\x"),dict(name=NAME+"x"),dict(expected_server_handle=True),
            dict(expected_server_handle=0),dict(timeout=0),dict(timeout=True),dict(timeout=6),
            dict(timeout=float("nan")),dict(deadline=10**1000),dict(deadline=float("inf")),dict(cancel=False),
            dict(prompt=replace(PROMPT,generation=0))]
        with patch("windows_native_command_review._VerifiedPipe") as opened:
            for changes in cases:
                options=dict(name=NAME,expected_server_handle=1,prompt=PROMPT);options.update(changes)
                with self.subTest(changes=changes),self.assertRaises(ValueError): receive_windows_command_decision(**options)
            opened.assert_not_called()

    @unittest.skipIf(os.name=="nt","unsupported-platform check")
    def test_valid_configuration_has_no_cross_platform_fallback(self):
        with self.assertRaises(ipc.BootstrapConnectionError): receive_windows_command_decision(NAME,1,PROMPT)


@unittest.skipUnless(os.name=="nt" and os.environ.get("IDW_COMMAND_REVIEW_PIPE_FIXTURE"),"Windows review fixture required")
class ReviewPipeTests(unittest.TestCase):
    def test_oversized_or_truncated_proposal_never_reaches_reviewer(self):
        for data in (b"\xff\xff",(200).to_bytes(2,"big")+b"IDWQ\x01"):
            host,name=self.host()
            connection=ipc._VerifiedPipe(name,int(host._handle),1,None,None)
            try: self.assertTrue(connection.transfer(bytearray(data),True))
            finally: connection.close()
            host.wait(timeout=3);self.assertEqual(host.returncode,3)
            self.assertNotIn(b"REVIEW",host.stdout.read())

    def host(self,mode="allow",pid=None):
        process=subprocess.Popen([os.environ["IDW_COMMAND_REVIEW_PIPE_FIXTURE"],"--test-host",str(pid or os.getpid()),mode],
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,bufsize=0,close_fds=True,
            creationflags=subprocess.CREATE_NO_WINDOW)
        def cleanup():
            if process.poll() is None: process.kill()
            process.communicate(timeout=3)
        self.addCleanup(cleanup)
        record=process.stdout.readline()
        self.assertTrue(record.startswith(b"PIPE "),record)
        return process,record[5:-1].decode("ascii")

    def test_exact_allow_and_deny_close_one_transaction_without_closing_borrowed_parent_handle(self):
        for mode,allow in (("allow",True),("deny",False)):
            with self.subTest(mode=mode):
                host,name=self.host(mode)
                self.assertIs(receive_windows_command_decision(name,int(host._handle),PROMPT),allow)
                host.wait(timeout=3);self.assertEqual(host.returncode,0)
                self.assertEqual(host.stdout.read(),b"REVIEW\nDONE\n")
                with self.assertRaises(ConnectionError):
                    receive_windows_command_decision(name,int(host._handle),PROMPT,timeout=.05)

    def test_wrong_tls_binding_or_peer_never_reaches_reviewer(self):
        for changes in (dict(epoch=46),dict(peer_id="other"),dict(peer_sha256="d"*64),
                        dict(connection_id="d"*64),dict(tls_version="TLSv1.2")):
            host,name=self.host()
            payload=PROMPT.payload.replace(b"|45|",b"|46|") if changes.get("epoch")==46 else PROMPT.payload
            prompt=replace(PROMPT,identity=replace(PROMPT.identity,**changes),payload=payload)
            with self.subTest(changes=changes),self.assertRaises(ConnectionError):
                receive_windows_command_decision(name,int(host._handle),prompt)
            host.wait(timeout=3);self.assertNotIn(b"REVIEW",host.stdout.read())

    def unrelated(self):
        process=subprocess.Popen([sys.executable,"-c","import time;time.sleep(15)"],
            close_fds=True,creationflags=subprocess.CREATE_NO_WINDOW)
        self.addCleanup(lambda: (process.kill(),process.wait(timeout=3)) if process.poll() is None else None)
        return process

    def test_wrong_held_server_receives_no_proposal_bytes(self):
        host,name=self.host();unrelated=self.unrelated()
        with patch.object(ipc,"_write",wraps=ipc._write) as write:
            with self.assertRaises(ipc.BootstrapConnectionError):
                receive_windows_command_decision(name,int(unrelated._handle),PROMPT)
            write.assert_not_called()
        host.wait(timeout=3);self.assertNotIn(b"REVIEW",host.stdout.read())

    def test_wrong_held_client_never_reaches_native_reviewer(self):
        unrelated=self.unrelated();host,name=self.host(pid=unrelated.pid)
        with self.assertRaises(ConnectionError): receive_windows_command_decision(name,int(host._handle),PROMPT)
        host.wait(timeout=3);self.assertNotIn(b"REVIEW",host.stdout.read())

    def test_cancel_expiry_and_callback_error_fail_closed(self):
        for mode in ("expire","throw"):
            host,name=self.host(mode)
            with self.subTest(mode=mode),self.assertRaises(ConnectionError):
                receive_windows_command_decision(name,int(host._handle),PROMPT)
            host.wait(timeout=3);self.assertEqual(host.returncode,3)
        host,name=self.host("cancel")
        connection=ipc._VerifiedPipe(name,int(host._handle),1,None,None)
        try:
            # Idle verified pipe waits for native cancellation, with no proposal.
            self.assertFalse(connection.transfer(bytearray(2),False))
        finally: connection.close()
        host.wait(timeout=3);self.assertEqual(host.returncode,3)

    def test_client_mid_io_cancellation_and_original_deadline_do_not_wait_for_slow_decision(self):
        for canceled in (True,False):
            host,name=self.host("slow");cancel=threading.Event()
            timer=threading.Timer(.05,cancel.set) if canceled else None
            if timer: timer.start()
            started=time.monotonic()
            try:
                with self.assertRaises(ConnectionError):
                    receive_windows_command_decision(name,int(host._handle),PROMPT,cancel=cancel,timeout=2,
                        deadline=None if canceled else time.monotonic()+.05)
            finally:
                if timer: timer.cancel();timer.join()
            self.assertLess(time.monotonic()-started,.75) # Return before the one-second fixture decision.
            host.wait(timeout=3);self.assertEqual(host.returncode,3)

    def test_client_cancellation_and_expired_deadline_write_nothing(self):
        for canceled in (True,False):
            host,name=self.host();cancel=threading.Event()
            if canceled: cancel.set()
            with patch.object(ipc,"_write",wraps=ipc._write) as write:
                with self.assertRaises(ipc.BootstrapConnectionError):
                    receive_windows_command_decision(name,int(host._handle),PROMPT,cancel=cancel,
                        deadline=None if canceled else time.monotonic()-1)
                write.assert_not_called()


if __name__=="__main__": unittest.main()
