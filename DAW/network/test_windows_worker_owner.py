"""Runs the actual Win32 process owner; executed on the Windows CI job only."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
import ssl
import test_tls_native_worker as tls_tests
from tls_client import verified_tls_client


class WindowsWorkerOwnerTests(unittest.TestCase):
    def run_child(self, body, *, mode="finish", payload=None):
        with tempfile.TemporaryDirectory(prefix="IDW worker spaces ") as folder:
            script=Path(folder)/"test child.py"
            script.write_text(body,encoding="utf-8")
            result=subprocess.run([os.environ["IDW_WINDOWS_WORKER_FIXTURE"],
                                   sys.executable,str(script),mode],
                                  input=json.dumps(payload or {"secret":"test-only-secret"})+"\n",
                                  text=True,capture_output=True,timeout=12)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(result.stderr,"")
            self.assertNotIn("test-only-secret",result.stdout)
            return result.stdout.strip()

    def test_binary_bootstrap_and_success(self):
        self.assertEqual(self.run_child('''import sys,json
n=int.from_bytes(sys.stdin.buffer.read(4),'big')
data=json.loads(sys.stdin.buffer.read(n))
assert data['secret']=='test-only-secret'
assert sys.stdin.buffer.read()==b''
sys.stdout.buffer.write(b'READY\\n');sys.stdout.buffer.flush()
'''),"SUCCESS")

    def test_invalid_ready_and_child_failure(self):
        for output in ("WRONG\\n", "READY\\r\\n"):
            self.assertEqual(self.run_child(
                "import sys\nsys.stdin.buffer.read()\nsys.stdout.buffer.write(b'"+output+"')\n"),"FAILED")
        self.assertEqual(self.run_child("import sys\nsys.stdin.buffer.read()\nsys.exit(1)\n"),"FAILED")

    def test_stop_ready_child(self):
        start=time.monotonic()
        self.assertEqual(self.run_child('''import sys,time
sys.stdin.buffer.read()
sys.stdout.buffer.write(b'READY\\n');sys.stdout.buffer.flush()
time.sleep(30)
''',mode="stop-ready"),"STOPPED")
        self.assertLess(time.monotonic()-start,5)

    def test_startup_timeout_kills_nonreading_child(self):
        start=time.monotonic()
        self.assertEqual(self.run_child("import time\ntime.sleep(30)\n",
                                       payload={"secret":"test-only-secret","padding":"x"*15000}),"FAILED")
        self.assertLess(time.monotonic()-start,9)

    def test_oversize_bootstrap_rejected(self):
        self.assertEqual(self.run_child("raise RuntimeError('must not run')\n",
                                       payload={"padding":"x"*17000}),"REJECTED")


class WindowsOwnerTLSIntegrationTests(tls_tests.TLSNativeWorkerTests):
    """Reuse transport assertions, but start the real worker through C++/Win32."""
    def exchange(self, payload=b"1|45|1|arm1|arm|0", **overrides):
        config=self.config(**overrides)
        script=Path(__file__).resolve().with_name("tls_native_worker.py")
        process=subprocess.Popen([os.environ["IDW_WINDOWS_WORKER_FIXTURE"],
                                  sys.executable,str(script),"notify-ready"],
                                 stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE,text=True,bufsize=1)
        try:
            process.stdin.write(json.dumps(config)+"\n")
            process.stdin.close()
            self.assertEqual(process.stdout.readline().strip(),"READY")
            try:
                reply=verified_tls_client("127.0.0.1",config["tls_port"],"localhost",
                                          config["server_cert"],payload,
                                          client_certfile=str(self.root/"guest.crt"),
                                          client_keyfile=str(self.root/"guest.key"))
            except (ConnectionError,ssl.SSLError,OSError):
                reply=None
            process.wait(timeout=10)
            outcome=process.stdout.read().strip()
            self.assertIn(outcome,("SUCCESS","FAILED"))
            self.assertEqual(process.stderr.read(),"")
            self.assertEqual(process.returncode,0)
            return reply,0 if outcome=="SUCCESS" else 1
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=3)
            process.stdin.close()
            process.stdout.close()
            process.stderr.close()


if __name__=="__main__":
    if sys.platform!="win32":
        raise SystemExit("This integration test requires Windows")
    unittest.main()
