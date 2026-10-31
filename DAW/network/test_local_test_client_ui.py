"""Tk widget/state checks; Windows CI creates a hidden window, not a live session."""
import tempfile
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
