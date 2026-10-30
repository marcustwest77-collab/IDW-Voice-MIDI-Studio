"""Private TLS offer -> host policy -> native lease; localhost, no GUI actions."""
from dataclasses import replace
import os
import subprocess
import sys
import unittest
from unittest.mock import patch
from native_tls_offer import NativeTLSOffer
from native_tls_session import TLSConnectionIdentity
from windows_native_bootstrap import receive_windows_bootstrap
from test_native_local_bridge import NativeFixture
import test_native_tls_receiver as receivers
import test_native_tls_session as sessions


class OfferTests(unittest.TestCase):
    def test_fixed_record_and_invalid_fields(self):
        offer = NativeTLSOffer(45, "a" * 64, "b" * 64, "TLSv1.3")
        record = offer.encode()
        self.assertEqual(len(record), 142)
        self.assertEqual(record, b"IDWO\x01" + (45).to_bytes(8, "big") + b"a" * 64 + b"b" * 64 + b"\x0d")
        for changes in (dict(epoch=True), dict(epoch=0), dict(peer_sha256="A" * 64),
                        dict(connection_id="bad"), dict(tls_version="TLSv1")):
            with self.assertRaises(ValueError): replace(offer, **changes).encode()

    def test_offer_must_match_expected_binding_before_any_pipe_access(self):
        with self.assertRaises(ValueError):
            receive_windows_bootstrap("\\\\.\\pipe\\IDW-LocalBootstrap-" + "a" * 64, 123,
                expected_epoch=45, expected_peer_sha256="a" * 64, expected_connection_id="b" * 64,
                consume=lambda _: self.fail("mismatch must not consume"),
                offer=NativeTLSOffer(46, "a" * 64, "b" * 64, "TLSv1.3"))

    @unittest.skipUnless(os.name == "nt" and os.environ.get("IDW_NATIVE_LOCAL_FIXTURE"), "Windows fixture required")
    def test_host_policy_mismatch_refuses_bootstrap(self):
        identity = TLSConnectionIdentity("guest", 45, "a" * 64, "b" * 64, "TLSv1.3")
        # Approved host pin is different from what the trusted test bridge offers.
        with self.assertRaises(ConnectionError):
            NativeFixture("0" * 64, "b" * 64, private_bootstrap=True,
                          private_offer=True, lease_identity=identity)

    @unittest.skipUnless(os.name == "nt" and os.environ.get("IDW_NATIVE_LOCAL_FIXTURE"), "Windows fixture required")
    def test_wrong_server_receives_no_offer_bytes(self):
        import windows_native_bootstrap as bootstrap
        unrelated = subprocess.Popen([sys.executable, "-c", "import time;time.sleep(10)"],
            close_fds=True, creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            def receive(name, _handle, **arguments):
                with patch.object(bootstrap, "_write") as write, patch.object(bootstrap, "_read") as read:
                    accepted = receive_windows_bootstrap(name, int(unrelated._handle),
                        **arguments, offer=NativeTLSOffer(45, "a" * 64, "b" * 64, "TLSv1.3"))
                    write.assert_not_called();read.assert_not_called()
                    return accepted
            with self.assertRaises(RuntimeError):
                NativeFixture("a" * 64, "b" * 64, private_bootstrap=True, receive=receive)
        finally:
            unrelated.terminate();unrelated.wait(timeout=2)


@unittest.skipUnless(os.name == "nt", "Windows private offer integration")
class PrivateOfferTLSIntegrationTests(receivers.OwnedReceiverIntegrationTests):
    def harness(self, **options):
        return sessions.SessionHarness(self.cert, self.key, self.guest_cert, self.guest_key,
            owned_receiver=True, private_bootstrap=True, transactional_lease=True, private_offer=True, **options)


if __name__ == "__main__": unittest.main()
