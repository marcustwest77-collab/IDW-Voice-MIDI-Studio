"""Real localhost mutual TLS handshake with throwaway certs."""
import shutil
import socket
import ssl
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path
from tls_client import verified_tls_client
from tls_loopback_server import serve_tls_once

@unittest.skipUnless(shutil.which("openssl"), "OpenSSL CLI needed")
class MutualTLSTest(unittest.TestCase):
    def test_client_certificate_required(self):
        with tempfile.TemporaryDirectory() as folder:
            def issue(name):
                cert=Path(folder)/(name+".crt")
                key=Path(folder)/(name+".key")
                subprocess.run(["openssl","req","-x509","-newkey","rsa:2048","-nodes",
                    "-keyout",str(key),"-out",str(cert),"-days","1",
                    "-subj",f"/CN={name}","-addext",f"subjectAltName=DNS:{name}"],
                    capture_output=True,check=True,timeout=25)
                return cert,key
            server_cert,server_key=issue("localhost")
            client_cert,client_key=issue("approved-client")
            with socket.socket() as probe:
                probe.bind(("127.0.0.1",0))
                port=probe.getsockname()[1]
            ready=threading.Event()
            errors=[]
            def server():
                try:
                    serve_tls_once(port,str(server_cert),str(server_key),
                                   ready=ready,client_cafile=str(client_cert))
                except (ssl.SSLError, ConnectionError, OSError) as exc:
                    errors.append(exc)
            worker=threading.Thread(target=server,daemon=True)
            worker.start()
            self.assertTrue(ready.wait(5))
            self.assertEqual(
                verified_tls_client("127.0.0.1",port,"localhost",str(server_cert),
                                    b"mutual-tls",client_certfile=str(client_cert),
                                    client_keyfile=str(client_key)),b"mutual-tls")
            worker.join(timeout=5)
            self.assertFalse(worker.is_alive())
            self.assertEqual(errors,[])

    def test_partial_credentials_rejected(self):
        with self.assertRaises(ValueError):
            verified_tls_client("localhost",9000,"localhost","ca.pem",b"hello",
                                client_certfile="cert.pem")
if __name__=="__main__":
    unittest.main()
