"""Real Python-to-native socket integration; no JUCE or Python receiver fixture."""
import os
import secrets
import socket
import subprocess
import sys
import time
import unittest
from native_local_command_bridge import NativeLocalCommandBridge
from verified_event_producer import VerifiedCommandEvent


class NativeReceiverTests(unittest.TestCase):
    def setUp(self):
        self.token = secrets.token_hex(32)
        self.proc = subprocess.Popen([os.environ["IDW_NATIVE_RECEIVER_FIXTURE"]],
                                     stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                     text=True, bufsize=1)
        self.proc.stdin.write(self.token + "\n")
        self.proc.stdin.flush()
        self.port = int(self.proc.stdout.readline())
        self.client = NativeLocalCommandBridge(self.port, session_token=self.token)

    def tearDown(self):
        if self.proc.poll() is None:
            self.proc.kill()
        self.proc.wait(timeout=5)
        self.proc.stdin.close()
        self.proc.stdout.close()

    def operation(self, command):
        self.proc.stdin.write(command + "\n")
        self.proc.stdin.flush()
        return self.proc.stdout.readline().strip()

    def event(self, seq=1, peer="peer", epoch=45, action="arm"):
        return VerifiedCommandEvent(epoch, f"1|{epoch}|{seq}|r{seq}|{action}|0", peer)

    def test_authentication_replay_and_delivery(self):
        wrong = NativeLocalCommandBridge(self.port, session_token=secrets.token_hex(32))
        self.assertFalse(self.client.deliver(self.event()))
        self.assertEqual(self.operation("admitted"), "no")
        self.assertFalse(wrong.connected("peer",45))
        self.assertFalse(self.client.connected("other",45))
        self.assertFalse(self.client.connected("peer",46))
        self.assertEqual(self.operation("admitted"), "no")
        self.assertTrue(self.client.connected("peer",45))
        self.assertEqual(self.operation("admitted"), "yes")
        self.assertFalse(wrong.deliver(self.event()))
        self.assertFalse(self.client.deliver(self.event(peer="other")))
        self.assertFalse(self.client.deliver(self.event(epoch=46)))
        self.assertFalse(self.client.deliver(self.event(action="record")))
        self.assertEqual(self.operation("drain"), "0")
        self.assertTrue(self.client.deliver(self.event()))
        self.assertFalse(self.client.deliver(self.event()))
        self.assertTrue(self.client.deliver(self.event(2)))
        self.assertEqual(self.operation("drain"), "2")
        self.assertTrue(self.client.deliver(self.event(3)))
        self.assertEqual(self.operation("revoke"), "revoked")
        self.assertFalse(self.client.connected("peer",45))
        self.assertFalse(self.client.deliver(self.event(4)))
        self.assertEqual(self.operation("drain"), "0")

    def test_queue_backpressure_does_not_ack_or_consume_sequence(self):
        self.assertTrue(self.client.connected("peer",45))
        self.operation("drain")
        for seq in range(1, 33):
            self.assertTrue(self.client.deliver(self.event(seq)))
        self.assertFalse(self.client.deliver(self.event(33)))
        self.assertEqual(self.operation("drain"), "32")
        self.assertTrue(self.client.deliver(self.event(33)))

    def test_bad_frame_and_slow_client_recovery(self):
        for header in (b"\0\0\0\0", (4097).to_bytes(4, "big")):
            with socket.create_connection(("127.0.0.1", self.port), 2) as connection:
                connection.sendall(header)
                self.assertEqual(connection.recv(1), b"")
        with socket.create_connection(("127.0.0.1", self.port), 2) as connection:
            connection.sendall(b"\0")
            self.assertEqual(connection.recv(1), b"")
        self.assertTrue(self.client.connected("peer",45))
        self.assertTrue(self.client.deliver(self.event()))

    def test_stop_interrupts_partial_frame(self):
        with socket.create_connection(("127.0.0.1", self.port), 2) as connection:
            connection.sendall(b"\0")
            start = time.monotonic()
            self.assertEqual(self.operation("stop"), "stopped")
            self.proc.wait(timeout=2)
            self.assertLess(time.monotonic() - start, 1.0)
        self.assertFalse(self.client.deliver(self.event()))


if __name__ == "__main__":
    unittest.main()
