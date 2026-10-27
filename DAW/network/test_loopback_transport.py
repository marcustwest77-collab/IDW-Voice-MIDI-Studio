#!/usr/bin/env python3
"""Offline integration test for loopback TCP echo; no external services."""
import socket
import threading
import unittest

import loopback_transport as t


class TransportTest(unittest.TestCase):
    def test_framing_and_partial_delivery(self):
        left, right = socket.socketpair()
        try:
            def deliver():
                t.send_frame(left, b"arm track 2")
            thread = threading.Thread(target=deliver)
            thread.start()
            self.assertEqual(t.receive_frame(right), b"arm track 2")
            thread.join(timeout=3)
        finally:
            left.close()
            right.close()

    def test_empty_and_oversized(self):
        left, right = socket.socketpair()
        try:
            with self.assertRaises(ValueError):
                t.send_frame(left, b"")
            with self.assertRaises(ValueError):
                t.send_frame(left, b"x" * 4097)
            right.sendall(bytes([255, 255, 255, 255]))
            with self.assertRaises(ValueError):
                t.receive_frame(left)
        finally:
            left.close()
            right.close()

    def test_local_tcp_round_trip(self):
        probe = socket.socket()
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
        probe.close()
        server = threading.Thread(target=t.serve_once, args=(port,), daemon=True)
        server.start()
        # A brief socket connect retry handles startup scheduling on CI.
        import time
        for _ in range(30):
            try:
                t.client(port, b"IDW local")
                break
            except OSError:
                time.sleep(0.025)
        else:
            self.fail("local TCP server did not accept a connection")
        server.join(timeout=3)
        self.assertFalse(server.is_alive())


if __name__ == "__main__":
    unittest.main()
