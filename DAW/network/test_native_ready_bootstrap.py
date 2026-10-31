"""Real Windows readiness pipe scopes; TLS identity is synthetic in these tests."""
import os
import ctypes
import subprocess
import sys
import threading
import time
import unittest
from unittest.mock import patch

import windows_native_bootstrap as ipc
from native_bridge_ready import NativeBridgeReady
from native_tls_session import TLSConnectionIdentity
from windows_native_lease import receive_windows_native_lease
from test_native_local_bridge import NativeFixture, binding
from verified_event_producer import VerifiedCommandEvent

NAME = "\\\\.\\pipe\\IDW-LocalBootstrap-" + "a"*64


class ReadyConfigurationTests(unittest.TestCase):
    @unittest.skipIf(os.name == "nt", "unsupported-platform boundary")
    def test_valid_ready_configuration_has_no_cross_platform_fallback(self):
        with self.assertRaises(ipc.BootstrapConnectionError):
            ipc.WindowsReadyBootstrap(NAME,1,NativeBridgeReady(45,12345))

    def test_invalid_ready_configuration_cannot_open_a_pipe(self):
        for arguments in (("bad", 1, NativeBridgeReady(45,12345)), (NAME, 0, NativeBridgeReady(45,12345)),
                          (NAME, 1, None)):
            with self.assertRaises(ValueError): ipc.WindowsReadyBootstrap(*arguments)

    @unittest.skipUnless(os.name == "nt", "Windows cancellation boundary")
    def test_canceled_ready_scope_never_opens_or_writes(self):
        cancel = threading.Event();cancel.set()
        with patch.object(ipc, "_open") as opened, patch.object(ipc, "_write") as write:
            with self.assertRaises(ipc.BootstrapConnectionError):
                ipc.WindowsReadyBootstrap(NAME,1,NativeBridgeReady(45,12345),cancel=cancel)
            opened.assert_not_called();write.assert_not_called()


class ReadyFixture(NativeFixture):
    def __init__(self):
        self.native_lease = None
        self.process = subprocess.Popen([os.environ["IDW_NATIVE_LOCAL_FIXTURE"], "--private-ready-offer",str(os.getpid())],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0,
            close_fds=True, creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
        try:
            self.process.stdin.write(("a"*64+"|"+"0"*64+"\n").encode());self.process.stdin.flush()
            record=self._read()
            if not record.startswith(b"PIPE "): raise RuntimeError("private readiness fixture unavailable")
            self.name=record[5:-1].decode("ascii")
        except BaseException:
            self.close();raise


@unittest.skipUnless(os.name == "nt" and os.environ.get("IDW_NATIVE_LOCAL_FIXTURE"),"Windows native fixture required")
class ReadyScopeTests(unittest.TestCase):
    def fixture(self):
        value=ReadyFixture();self.addCleanup(value.close);return value

    def test_one_open_pipe_across_readiness_offer_bootstrap_and_native_revocation(self):
        fixture=self.fixture();identity=TLSConnectionIdentity("guest",45,"a"*64,"b"*64,"TLSv1.3")
        with patch.object(ipc,"_open",wraps=ipc._open) as opened:
            with ipc.WindowsReadyBootstrap(fixture.name,int(fixture.process._handle),NativeBridgeReady(45,12345)) as ready:
                self.assertEqual(fixture._read(),b"READY_ACCEPTED\n")
                opens=opened.call_count;self.assertGreater(opens,0)
                lease=receive_windows_native_lease(fixture.name,int(fixture.process._handle),identity,
                                                  send_offer=True,ready_channel=ready)
                self.addCleanup(lease.close)
                self.assertEqual(opened.call_count,opens)
                fixture.bootstrap=lease.bootstrap
                bridge=binding(lease.bootstrap);self.addCleanup(bridge.close)
                self.assertTrue(bridge.deliver(VerifiedCommandEvent(45,"1|45|1|ready_scope|arm|1","guest")))
                self.assertEqual(fixture.pending(),1)
                lease.close();self.assertEqual(fixture.pending(),0)
                with self.assertRaises(RuntimeError):
                    receive_windows_native_lease(fixture.name,int(fixture.process._handle),identity,
                                                 send_offer=True,ready_channel=ready)

    def test_expired_readiness_deadline_cannot_be_renewed_by_provisioning(self):
        fixture=self.fixture();identity=TLSConnectionIdentity("guest",45,"a"*64,"b"*64,"TLSv1.3")
        with patch.object(ipc,"_write",wraps=ipc._write) as write:
            with ipc.WindowsReadyBootstrap(fixture.name,int(fixture.process._handle),NativeBridgeReady(45,12345),timeout=.15) as ready:
                self.assertEqual(fixture._read(),b"READY_ACCEPTED\n")
                writes=write.call_count;time.sleep(.2)
                with self.assertRaises(ConnectionError):
                    receive_windows_native_lease(fixture.name,int(fixture.process._handle),identity,
                                                 timeout=30,send_offer=True,ready_channel=ready)
                self.assertEqual(write.call_count,writes)

    def test_wrong_held_server_is_rejected_before_any_readiness_write(self):
        fixture=self.fixture()
        unrelated=subprocess.Popen([sys.executable,"-c","import time;time.sleep(5)"],
            creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
        try:
            with patch.object(ipc,"_write",wraps=ipc._write) as write:
                with self.assertRaises(ipc.BootstrapConnectionError):
                    ipc.WindowsReadyBootstrap(fixture.name,int(unrelated._handle),NativeBridgeReady(45,12345))
                write.assert_not_called()
        finally:
            unrelated.terminate();unrelated.wait(timeout=3)

    def test_failed_ack_rolls_back_a_constructed_lease_on_the_prepared_pipe(self):
        fixture=self.fixture();identity=TLSConnectionIdentity("guest",45,"a"*64,"b"*64,"TLSv1.3")
        original=ipc._write
        def fail_ack(pipe,data,*arguments):
            if bytes(data)==ipc._ACK:
                ctypes.set_last_error(5);return False
            return original(pipe,data,*arguments)
        with patch.object(ipc,"_write",side_effect=fail_ack), \
                patch("windows_native_lease.native_socket_lease") as factory:
            with ipc.WindowsReadyBootstrap(fixture.name,int(fixture.process._handle),NativeBridgeReady(45,12345)) as ready:
                self.assertEqual(fixture._read(),b"READY_ACCEPTED\n")
                with self.assertRaises(ConnectionError):
                    receive_windows_native_lease(fixture.name,int(fixture.process._handle),identity,
                                                 send_offer=True,ready_channel=ready)
                factory.return_value.close.assert_called_once()


if __name__ == "__main__": unittest.main()
