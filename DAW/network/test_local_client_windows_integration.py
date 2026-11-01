"""Real Tk Send -> Win32-owned TLS worker -> native queue, without approving edits."""
from contextlib import contextmanager
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
import time
import tkinter as tk
import unittest
from local_test_client import FIELDS
from local_test_client_ui import LocalTestClient
import test_tls_native_worker as tls_tests


class LocalClientWindowsIntegrationTests(unittest.TestCase):
    setUpClass = classmethod(tls_tests.TLSNativeWorkerTests.setUpClass.__func__)
    config = tls_tests.TLSNativeWorkerTests.config
    operation = tls_tests.TLSNativeWorkerTests.operation
    close_native = tls_tests.TLSNativeWorkerTests.close_native

    def setUp(self):
        tls_tests.TLSNativeWorkerTests.setUp(self)
        self.window = tk.Tk()
        self.window.withdraw()
        self.client = LocalTestClient(self.window)
        self.addCleanup(self.client.close)

    @contextmanager
    def worker(self, config):
        script = Path(__file__).resolve().with_name("tls_native_worker.py")
        process = subprocess.Popen([os.environ["IDW_WINDOWS_WORKER_FIXTURE"],
                                    sys.executable, str(script), "notify-ready"],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True, bufsize=1)
        try:
            process.stdin.write(json.dumps(config) + "\n")
            process.stdin.close()
            ready = queue.Queue()
            threading.Thread(target=lambda: ready.put(process.stdout.readline().strip()),
                             daemon=True).start()
            self.assertEqual(ready.get(timeout=10), "READY")
            yield process
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
            process.stdin.close()
            process.stdout.close()
            process.stderr.close()

    def send_from_window(self, config, action):
        setup = {key: config[key] for key in FIELDS}
        self.client.use_setup(setup, self.root / "local-test-setup.json",
                              str(self.root / "guest.crt"), str(self.root / "guest.key"))
        self.client.epoch.set(str(config["epoch"]))
        self.client.action.set(action)
        self.client.track.set("1")
        self.client.send_button.invoke()
        self.assertTrue(self.client.pending)
        self.assertEqual(str(self.client.send_button["state"]), "disabled")
        deadline = time.monotonic() + 10
        while self.client.pending and time.monotonic() < deadline:
            self.window.update()
            time.sleep(0.01)
        self.assertFalse(self.client.pending, "client did not finish its bounded request")
        self.assertEqual(str(self.client.send_button["state"]), "normal")

    def successful_request(self, action):
        config = self.config()
        with self.worker(config) as process:
            self.send_from_window(config, action)
            process.wait(timeout=10)
            self.assertEqual(process.stdout.read().strip(), "SUCCESS")
            self.assertEqual(process.stderr.read(), "")
            self.assertEqual(process.returncode, 0)
        self.assertIn("Request queued", self.client.status.get())
        self.assertIn("no track action has been approved", self.client.status.get())
        self.assertEqual(self.client.sequence.get(), "2")
        self.assertEqual(self.operation("admitted"), "yes")
        self.assertEqual(self.operation("drain"), "1")
        self.assertEqual(self.operation("drain"), "0")

    def test_select_button_delivers_one_pending_request(self):
        self.successful_request("Select track")

    def test_arm_button_delivers_one_pending_request(self):
        self.successful_request("Arm track")

    def test_wrong_pin_reports_failure_and_queues_nothing(self):
        config = self.config(client_pin="0" * 64)
        with self.worker(config) as process:
            self.send_from_window(config, "Select track")
            process.wait(timeout=10)
            self.assertEqual(process.stdout.read().strip(), "FAILED")
            self.assertEqual(process.stderr.read(), "")
            self.assertEqual(process.returncode, 0)
        self.assertIn("not accepted", self.client.status.get())
        self.assertNotIn("Request queued", self.client.status.get())
        self.assertEqual(self.client.sequence.get(), "1")
        self.assertEqual(self.operation("admitted"), "no")
        self.assertEqual(self.operation("drain"), "0")


if __name__ == "__main__":
    if sys.platform != "win32":
        raise SystemExit("This integration test requires Windows")
    unittest.main()
