import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from local_test_client import build_request, load_setup, send_local_request, validate_setup


class LocalTestClientTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.cert = Path(self.folder.name) / "test.pem"
        self.cert.write_text("test-only placeholder")
        self.setup = dict(version=1, peer="guest", tls_port=49200,
                          server_cert=str(self.cert), server_key=str(self.cert),
                          client_ca=str(self.cert), client_pin="a"*64,
                          approved_requests=["test1"])
        self.request = dict(epoch=45, sequence=1, request_id="test1", action="arm", track_number=1)

    def test_load_and_track_mapping(self):
        source = Path(self.folder.name) / "setup.json"
        source.write_text(json.dumps(self.setup))
        self.assertEqual(load_setup(source), self.setup)
        self.assertEqual(build_request(self.setup, **self.request), b"1|45|1|test1|arm|0")
        self.assertEqual(build_request(self.setup, **dict(self.request, track_number=128)),
                         b"1|45|1|test1|arm|127")

    def test_duplicate_oversize_and_unknown_setup_fields(self):
        source = Path(self.folder.name) / "setup.json"
        for contents in (json.dumps(self.setup)[:-1]+',"peer":"other"}', "x"*16385,
                         json.dumps(dict(self.setup, host="external.example")),
                         json.dumps(dict(self.setup, token="never-load"))):
            source.write_text(contents)
            with self.assertRaises(ValueError):
                load_setup(source)

    def test_invalid_setup_and_requests_do_not_connect(self):
        with patch("local_test_client.verified_tls_client") as connect:
            for key, value in (("epoch",0), ("epoch",True), ("sequence",2**64),
                               ("sequence",False), ("request_id","unapproved"),
                               ("action","record"), ("track_number",0),
                               ("track_number",129), ("track_number",True)):
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    send_local_request(self.setup,str(self.cert),str(self.cert),
                                       **dict(self.request,**{key:value}))
            for key, value in (("version",True), ("tls_port",True), ("tls_port",65536),
                               ("client_pin","invalid"), ("peer","bad|peer"),
                               ("approved_requests",["duplicate"]*2)):
                with self.subTest(key=key), self.assertRaises(ValueError):
                    validate_setup(dict(self.setup,**{key:value}))
            connect.assert_not_called()

    def test_verified_localhost_acknowledgement(self):
        wire=build_request(self.setup,**self.request)
        with patch("local_test_client.verified_tls_client",return_value=wire) as connect:
            result=send_local_request(self.setup,str(self.cert),str(self.cert),**self.request)
            self.assertIn("queued",result)
            self.assertIn("no track action has been approved",result)
            connect.assert_called_once_with("127.0.0.1",49200,"localhost",str(self.cert),wire,
                                            timeout=5,client_certfile=str(self.cert),
                                            client_keyfile=str(self.cert))

    def test_wrong_reply_and_missing_key_fail(self):
        with patch("local_test_client.verified_tls_client",return_value=b"different"):
            with self.assertRaises(PermissionError):
                send_local_request(self.setup,str(self.cert),str(self.cert),**self.request)
        with patch("local_test_client.verified_tls_client") as connect:
            with self.assertRaises(ValueError):
                send_local_request(self.setup,str(self.cert),"relative.key",**self.request)
            connect.assert_not_called()


if __name__=="__main__":
    unittest.main()
