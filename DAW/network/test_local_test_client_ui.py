"""Tk widget/state checks; Windows CI creates a hidden window, not a live session."""
import tempfile
import threading
from contextlib import ExitStack
from pathlib import Path
import time
import tkinter as tk
import unittest
from unittest.mock import patch
from local_test_client_ui import LocalTestClient


class ClientUITests(unittest.TestCase):
    def setUp(self):
        self.root=tk.Tk()
        self.root.withdraw()
        self.client=LocalTestClient(self.root)
        self.addCleanup(self.client.close)
        self.folder=tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        cert=Path(self.folder.name)/"test.pem"
        cert.write_text("test-only placeholder")
        self.client.setup=dict(version=1,peer="guest",tls_port=49200,server_cert=str(cert),
                               server_key=str(cert),client_ca=str(cert),client_pin="a"*64,
                               approved_requests=["test1"])
        self.client.client_cert=self.client.client_key=str(cert)
        self.client.request_id.set("test1")

    def wait_result(self):
        until=time.monotonic()+3
        while self.client.pending and time.monotonic()<until:
            self.root.update()
            time.sleep(0.01)
        self.assertFalse(self.client.pending)

    def test_success_disables_then_restores_controls(self):
        with patch("local_test_client_ui.send_local_request",return_value="Request queued. Review it in the DAW.") as send:
            self.client.send()
            self.assertTrue(self.client.pending)
            self.assertEqual(str(self.client.send_button["state"]),"disabled")
            self.client.send() # A double click must not send another request.
            self.wait_result()
            send.assert_called_once()
        self.assertIn("queued",self.client.status.get())
        self.assertEqual(self.client.sequence.get(),"2")
        self.assertEqual(str(self.client.requests["state"]),"readonly")

    def test_failure_is_not_reported_as_success(self):
        with patch("local_test_client_ui.send_local_request",side_effect=OSError("private detail")):
            self.client.send()
            self.wait_result()
        self.assertIn("not accepted",self.client.status.get())
        self.assertNotIn("private detail",self.client.status.get())
        self.assertEqual(self.client.sequence.get(),"1")

    def generation_dialogs(self, stack, confirm=True):
        stack.enter_context(patch("local_test_client_ui.filedialog.askopenfilename", return_value="openssl.exe"))
        stack.enter_context(patch("local_test_client_ui.filedialog.askdirectory", return_value=self.folder.name))
        stack.enter_context(patch("local_test_client_ui.messagebox.askyesno", return_value=confirm))

    def test_generate_loads_credentials_without_sending_or_sequence_change(self):
        release = threading.Event()
        started = threading.Event()
        path = Path(self.folder.name) / "new" / "local-test-setup.json"
        def generate(*args):
            started.set()
            if not release.wait(2):
                raise RuntimeError("test generation timed out")
            return path
        setup = dict(self.client.setup)
        with ExitStack() as stack:
            self.generation_dialogs(stack)
            create = stack.enter_context(patch("local_test_client_ui.create_bundle", side_effect=generate))
            stack.enter_context(patch("local_test_client_ui.load_setup", return_value=setup))
            send = stack.enter_context(patch("local_test_client_ui.send_local_request"))
            self.client.generate_certificates()
            self.assertTrue(started.wait(1))
            try:
                self.assertTrue(self.client.pending)
                self.assertEqual(str(self.client.send_button["state"]), "disabled")
                self.client.generate_certificates()
                self.client.send()
                self.client.close()
                self.assertFalse(self.client.closed)
                create.assert_called_once()
                send.assert_not_called()
            finally:
                release.set()
            self.wait_result()
        self.assertFalse(self.client.generating)
        self.assertEqual(self.client.client_key, str(path.parent / "guest.key"))
        self.assertEqual(self.client.sequence.get(), "1")
        self.assertIn(str(path), self.client.status.get())
        self.assertEqual(str(self.client.requests["state"]), "readonly")

    def test_generate_failure_preserves_setup(self):
        original = self.client.setup
        key = self.client.client_key
        with ExitStack() as stack:
            self.generation_dialogs(stack)
            stack.enter_context(patch("local_test_client_ui.create_bundle", side_effect=OSError("private detail")))
            self.client.generate_certificates()
            self.wait_result()
        self.assertIs(self.client.setup, original)
        self.assertEqual(self.client.client_key, key)
        self.assertIn("failed", self.client.status.get())
        self.assertNotIn("private detail", self.client.status.get())
        self.assertFalse(self.client.generating)

    def test_generate_cancel_does_not_create_files(self):
        with ExitStack() as stack:
            self.generation_dialogs(stack, confirm=False)
            create = stack.enter_context(patch("local_test_client_ui.create_bundle"))
            self.client.generate_certificates()
            create.assert_not_called()
        self.assertFalse(self.client.pending)

    def test_create_setup_loads_result_without_network(self):
        setup = dict(self.client.setup)
        with patch("local_test_client_ui.simpledialog.askstring", return_value="guest"), \
             patch("local_test_client_ui.simpledialog.askinteger", return_value=49200), \
             patch("local_test_client_ui.filedialog.askopenfilename", side_effect=["server.crt", "server.key", "ca.crt", "client.crt", "client.key"]), \
             patch("local_test_client_ui.filedialog.asksaveasfilename", return_value="new.json"), \
             patch("local_test_client_ui.create_local_setup", return_value=setup) as create, \
             patch("local_test_client_ui.send_local_request") as send:
            self.client.create_setup()
            create.assert_called_once_with("new.json", peer="guest", tls_port=49200,
                                           server_cert="server.crt", server_key="server.key", client_ca="ca.crt",
                                           client_cert="client.crt", client_key="client.key")
            send.assert_not_called()
        self.assertEqual(self.client.client_key, "client.key")
        self.assertIn("saved and loaded", self.client.status.get())
        self.assertEqual(self.client.request_id.get(), "test1")

    def test_cancel_create_preserves_loaded_setup(self):
        original = self.client.setup
        with patch("local_test_client_ui.simpledialog.askstring", return_value=None), \
             patch("local_test_client_ui.create_local_setup") as create:
            self.client.create_setup()
            create.assert_not_called()
        self.assertIs(self.client.setup, original)

    def test_preflight_reports_result_without_sending(self):
        with patch("local_test_client_ui.check_local_setup", return_value="No connection was made.") as check, \
             patch("local_test_client_ui.send_local_request") as send:
            self.client.check_setup()
            check.assert_called_once()
            send.assert_not_called()
        self.assertIn("No connection", self.client.status.get())
        self.assertEqual(self.client.sequence.get(), "1")
        with patch("local_test_client_ui.check_local_setup", side_effect=ValueError("private detail")):
            self.client.check_setup()
        self.assertIn("failed", self.client.status.get())
        self.assertNotIn("private detail", self.client.status.get())

    def test_invalid_request_never_starts_network(self):
        self.client.epoch.set("invalid")
        with patch("local_test_client_ui.messagebox.showerror") as error, \
             patch("local_test_client_ui.send_local_request") as send:
            self.client.send()
            error.assert_called_once()
            send.assert_not_called()
        self.assertFalse(self.client.pending)


if __name__=="__main__":
    unittest.main()
