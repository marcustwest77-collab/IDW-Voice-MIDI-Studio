"""Real localhost accept/handshake/session ownership; no DAW execution."""
import socket
import ssl
import threading
import time
import unittest
from unittest.mock import patch

from native_tls_receiver import NativeTLSLoopbackReceiver
import test_native_tls_session as sessions


def context():
    result = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    result.minimum_version = ssl.TLSVersion.TLSv1_2
    result.verify_mode = ssl.CERT_REQUIRED
    return result


class ReceiverBoundaryTests(unittest.TestCase):
    def test_invalid_configuration_never_opens_socket(self):
        optional = context();optional.verify_mode = ssl.CERT_OPTIONAL
        legacy = context();legacy.minimum_version = ssl.TLSVersion.MINIMUM_SUPPORTED
        with patch("native_tls_receiver.socket.socket") as opened:
            for bad in (None, optional, legacy, ssl.create_default_context()):
                with self.assertRaises(ValueError): NativeTLSLoopbackReceiver(bad)
            for value in (0, True, 11, float("nan"), float("inf"), 10**1000):
                with self.assertRaises(ValueError): NativeTLSLoopbackReceiver(context(), accept_timeout=value)
                with self.assertRaises(ValueError): NativeTLSLoopbackReceiver(context(), handshake_timeout=value)
            opened.assert_not_called()

    def start(self, receiver):
        errors, results = [], []
        def serve():
            try: results.append(receiver.serve_once())
            except Exception as error: errors.append(error)
        worker = threading.Thread(target=serve)
        worker.start()
        self.addCleanup(lambda: worker.join(timeout=2))
        self.addCleanup(receiver.close)
        until = time.monotonic() + 1
        while time.monotonic() < until:
            with receiver._lock:
                if receiver._state != "ready":
                    break
            time.sleep(.005)
        self.assertNotEqual(receiver._state, "ready")
        return worker, errors, results

    def test_idle_accept_deadline_and_single_use(self):
        with NativeTLSLoopbackReceiver(context(), accept_timeout=.1) as receiver:
            self.assertEqual(receiver._listener.getsockname()[0], "127.0.0.1")
            with self.assertRaises(TimeoutError): receiver.serve_once()
            with self.assertRaises(RuntimeError): receiver.serve_once()
            self.assertEqual(receiver._listener.fileno(), -1)

    def test_close_before_serve_and_cancel_during_accept(self):
        receiver = NativeTLSLoopbackReceiver(context());receiver.close();receiver.close()
        with self.assertRaises(RuntimeError): receiver.serve_once()
        with NativeTLSLoopbackReceiver(context()) as receiver:
            worker, errors, results = self.start(receiver)
            receiver.close();worker.join(timeout=1)
            self.assertFalse(worker.is_alive());self.assertEqual(errors, []);self.assertEqual(results, [0])

    def test_stalled_handshake_deadline_closes_socket_without_native_setup(self):
        with NativeTLSLoopbackReceiver(context(), handshake_timeout=.15) as receiver:
            worker, errors, results = self.start(receiver)
            with socket.create_connection(("127.0.0.1", receiver.port), timeout=1) as raw:
                worker.join(timeout=1)
                self.assertFalse(worker.is_alive());self.assertEqual(results, [])
                self.assertEqual(len(errors), 1);self.assertIsInstance(errors[0], TimeoutError)
                raw.settimeout(1);self.assertEqual(raw.recv(1), b"")

    def test_cancel_during_handshake_and_no_second_serve(self):
        with NativeTLSLoopbackReceiver(context()) as receiver:
            worker, errors, results = self.start(receiver)
            with socket.create_connection(("127.0.0.1", receiver.port), timeout=1):
                # The accepted listener closes before TLS bytes are read.
                until = time.monotonic() + 1
                while receiver._listener.fileno() != -1 and time.monotonic() < until:
                    time.sleep(.005)
                self.assertEqual(receiver._listener.fileno(), -1)
                with self.assertRaises(RuntimeError): receiver.serve_once()
                receiver.close();worker.join(timeout=1)
            self.assertFalse(worker.is_alive());self.assertEqual(errors, []);self.assertEqual(results, [0])

    def test_context_downgrade_before_serve_closes_listener(self):
        configured = context()
        with NativeTLSLoopbackReceiver(configured) as receiver:
            configured.verify_mode = ssl.CERT_OPTIONAL
            with self.assertRaises(ValueError): receiver.serve_once()
            self.assertEqual(receiver._listener.fileno(), -1)

    def test_partial_tls_bytes_do_not_restart_handshake_deadline(self):
        with NativeTLSLoopbackReceiver(context(), handshake_timeout=.3) as receiver:
            worker, errors, results = self.start(receiver)
            with socket.create_connection(("127.0.0.1", receiver.port), timeout=1) as raw:
                raw.sendall(b"\x16")
                time.sleep(.12)
                raw.sendall(b"\x03")
                worker.join(timeout=1)
            self.assertFalse(worker.is_alive());self.assertEqual(results, [])
            self.assertEqual(len(errors), 1);self.assertIsInstance(errors[0], TimeoutError)


class OwnedReceiverIntegrationTests(sessions.LiveNativeTLSTests):
    def harness(self, **options):
        return sessions.SessionHarness(self.cert, self.key, self.guest_cert, self.guest_key,
                                       owned_receiver=True, **options)

    def test_bad_pin_missing_certificate_and_host_denial_create_no_native_lease(self):
        for options, credentials in ((dict(pin="0" * 64), True), ({}, False), (dict(allowed=False), True)):
            with self.subTest(options=options, credentials=credentials), self.harness(**options) as harness:
                try: harness.connect(credentials=credentials).close()
                except OSError: pass
                harness.join()
                self.assertIsNone(harness.fixture)
                self.assertEqual(harness.revocations, 0)
                self.assertTrue(harness.errors)


if __name__ == "__main__": unittest.main()
