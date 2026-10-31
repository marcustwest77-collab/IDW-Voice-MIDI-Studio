import copy
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from tls_worker_bootstrap import (MAX_BOOTSTRAP, TLSWorkerProcess, encode_bootstrap,
                                  fresh_session_secret, read_bootstrap)


class BootstrapTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        path = Path(self.folder.name) / "cert.pem"
        path.write_text("not a real certificate")
        self.config = dict(version=1, native_port=32001, tls_port=32002,
                           token=fresh_session_secret(), peer="peer", epoch=45,
                           channel="test_channel_binding_12345", server_cert=str(path),
                           server_key=str(path), client_ca=str(path), client_pin="a"*64,
                           host_approved=True, track_control=True, approved_requests=["arm1"])

    def test_roundtrip_and_fresh_secret(self):
        self.assertEqual(read_bootstrap(io.BytesIO(encode_bootstrap(self.config))), self.config)
        self.assertRegex(self.config["token"], r"^[a-f0-9]{64}$")
        self.assertNotEqual(self.config["token"], fresh_session_secret())

    def test_invalid_config_never_launches(self):
        invalid = [("version", True), ("epoch", True), ("epoch", 2**64),
                   ("native_port", 0), ("tls_port", 32001), ("token", "short"),
                   ("client_pin", "x"*64), ("peer", "peer\nother"),
                   ("channel", "short"), ("host_approved", False),
                   ("track_control", 1), ("approved_requests", []),
                   ("approved_requests", ["r"]*2), ("approved_requests", ["r"]*33),
                   ("approved_requests", ["bad|id"]), ("server_key", "relative.key")]
        with patch("tls_worker_bootstrap.subprocess.Popen") as spawn:
            for field, value in invalid:
                with self.subTest(field=field, value=value):
                    config = copy.deepcopy(self.config)
                    config[field] = value
                    with self.assertRaises((ValueError, PermissionError)):
                        TLSWorkerProcess(config)
            config = dict(self.config, extra=True)
            with self.assertRaises(ValueError):
                TLSWorkerProcess(config)
            spawn.assert_not_called()

    def test_malformed_frames(self):
        encoded = encode_bootstrap(self.config)
        duplicate = json.dumps(self.config)[:-1] + ',"token":"other"}'
        duplicate = duplicate.encode()
        for frame in (b"", b"\0\0", (MAX_BOOTSTRAP+1).to_bytes(4, "big"),
                      b"\0\0\0\0", encoded[:-1], encoded+b"extra",
                      len(duplicate).to_bytes(4, "big")+duplicate,
                      b"\0\0\0\1\xff"):
            with self.subTest(size=len(frame)):
                with self.assertRaises(ValueError):
                    read_bootstrap(io.BytesIO(frame))

    def test_startup_failure_is_generic(self):
        with self.assertRaisesRegex(RuntimeError, "^TLS worker startup failed$") as raised:
            TLSWorkerProcess(self.config)
        self.assertNotIn(self.config["token"], str(raised.exception))


if __name__ == "__main__":
    unittest.main()
