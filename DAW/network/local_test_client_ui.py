"""Manual test UI. Credentials stay in memory; network calls run off the UI thread."""
from pathlib import Path
import secrets
from create_local_test_bundle import create_bundle
import queue
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk
from local_test_client import build_request, check_local_setup, create_local_setup, load_setup, send_local_request


class LocalTestClient:
    def __init__(self, root):
        self.root = root
        self.setup = None
        self.setup_path = tk.StringVar(value="")
        self.client_cert = self.client_key = ""
        self.pending = False
        self.closed = False
        self.results = queue.Queue()
        self.generation_results = queue.Queue()
        self.generating = False
        root.title("IDW Remote Studio - Local Test Client")
        root.minsize(720, 680)
        frame = ttk.Frame(root, padding=22)
        frame.pack(fill="both", expand=True)
        frame.columnconfigure(1, weight=1)
        ttk.Label(frame, text="IDW Remote Studio", font=("Segoe UI", 20, "bold")).grid(
            row=0, column=0, columnspan=2, sticky="w", pady=(0, 6))
        ttk.Label(frame, text="One local request. Review and approve it in the DAW.").grid(
            row=1, column=0, columnspan=2, sticky="w", pady=(0, 18))
        self.summary = tk.StringVar(value="Load the same test setup selected in the DAW.")
        self.cert_label = tk.StringVar(value="No client certificate selected")
        self.key_label = tk.StringVar(value="No client key selected")
        self.controls = []
        self.copy_path_button = ttk.Button(frame, text="COPY SETUP PATH", command=self.copy_setup_path, state="disabled")
        self.copy_path_button.grid(row=0, column=2, sticky="e")
        self.controls.append((self.copy_path_button, "normal"))
        create = ttk.Button(frame, text="CREATE SETUP…", command=self.create_setup)
        create.grid(row=1, column=2, sticky="e", pady=(0, 18))
        self.controls.append((create, "normal"))
        for row, label, text, command in (
                (2, "Test setup", self.summary, self.choose_setup),
                (3, "Client certificate", self.cert_label, lambda: self.choose_credential(False)),
                (4, "Client private key", self.key_label, lambda: self.choose_credential(True))):
            ttk.Label(frame, text=label).grid(row=row, column=0, sticky="w", padx=(0, 12), pady=6)
            ttk.Label(frame, textvariable=text, wraplength=330).grid(row=row, column=1, sticky="w")
            button = ttk.Button(frame, text="Choose…", command=command)
            button.grid(row=row, column=2, sticky="e", padx=(12, 0))
            self.controls.append((button, "normal"))
        self.epoch = tk.StringVar(value="1")
        self.sequence = tk.StringVar(value="1")
        self.request_id = tk.StringVar()
        self.action = tk.StringVar(value="Select track")
        self.track = tk.StringVar(value="1")
        fields = (("Session epoch", self.epoch), ("Sequence", self.sequence),
                  ("Request ID", self.request_id), ("Action", self.action), ("Track number", self.track))
        for row, (label, variable) in enumerate(fields, 5):
            ttk.Label(frame, text=label).grid(row=row, column=0, sticky="w", pady=5)
            if variable is self.request_id:
                editor = ttk.Combobox(frame, textvariable=variable, state="readonly")
                self.requests = editor
            elif variable is self.action:
                editor = ttk.Combobox(frame, textvariable=variable, state="readonly",
                                      values=("Select track", "Arm track"))
            else:
                editor = ttk.Entry(frame, textvariable=variable)
            editor.grid(row=row, column=1, columnspan=2, sticky="ew")
            self.controls.append((editor, "readonly" if isinstance(editor, ttk.Combobox) else "normal"))
        self.check_button = ttk.Button(frame, text="CHECK SETUP (NO CONNECTION)", command=self.check_setup)
        self.check_button.grid(row=10, column=0, columnspan=3, sticky="ew", pady=(12, 0))
        self.controls.append((self.check_button, "normal"))
        self.send_button = ttk.Button(frame, text="SEND REQUEST FOR HOST REVIEW", command=self.send)
        self.send_button.grid(row=11, column=0, columnspan=3, sticky="ew", pady=(16, 10))
        self.status = tk.StringVar(value="Start the local test in the DAW, then copy its session epoch here.")
        ttk.Label(frame, textvariable=self.status, wraplength=580).grid(
            row=12, column=0, columnspan=3, sticky="w")
        generate = ttk.Button(frame, text="CREATE TEST CERTIFICATES…", command=self.generate_certificates)
        generate.grid(row=13, column=0, columnspan=3, sticky="ew", pady=(16, 0))
        self.controls.append((generate, "normal"))
        path_entry = ttk.Entry(frame, textvariable=self.setup_path, state="readonly")
        path_entry.grid(row=14, column=0, columnspan=3, sticky="ew", pady=(12, 0))
        self.controls.append((path_entry, "readonly"))
        self.poll_id = root.after(100, self.poll)
        root.protocol("WM_DELETE_WINDOW", self.close)

    def set_busy(self, busy):
        self.pending = busy
        for control, state in self.controls:
            disabled = busy or (control is self.copy_path_button and not self.setup_path.get())
            control.configure(state="disabled" if disabled else state)
        self.send_button.configure(state="disabled" if busy else "normal")

    def copy_setup_path(self):
        if self.pending or not self.setup_path.get():
            return
        try:
            self.root.clipboard_clear()
            self.root.clipboard_append(self.setup_path.get())
            self.status.set("Setup path copied. In the DAW, choose LOAD TEST SETUP and paste it into the file picker.")
        except tk.TclError:
            self.status.set("Clipboard unavailable. Select and copy the setup path from the field below.")

    def use_setup(self, setup, path, client_cert="", client_key=""):
        self.setup = setup
        self.setup_path.set(str(Path(path).resolve()))
        self.requests.configure(values=setup["approved_requests"])
        self.request_id.set(setup["approved_requests"][0])
        self.summary.set(f"{setup['peer']} | localhost:{setup['tls_port']}")
        self.client_cert, self.client_key = client_cert, client_key
        self.cert_label.set("Client certificate selected" if client_cert else "No client certificate selected")
        self.key_label.set("Client key selected (kept in its file)" if client_key else "No client key selected")
        self.epoch.set("")
        self.sequence.set("1")
        self.copy_path_button.configure(state="normal")

    def generate_certificates(self):
        if self.pending:
            return
        executable = filedialog.askopenfilename(title="Select your trusted OpenSSL executable", parent=self.root)
        if not executable:
            return
        parent = filedialog.askdirectory(title="Choose a private folder for disposable test credentials", parent=self.root)
        if not parent:
            return
        destination = Path(parent) / ("IDW-Local-Test-" + secrets.token_hex(4))
        if not messagebox.askyesno("Create local test certificates",
                "Run this OpenSSL executable?\n" + executable + "\n\nCreate seven-day test credentials in:\n" + str(destination) +
                "\n\nKeys are unencrypted. Keep this folder private. Nothing is installed in Windows certificate stores.", parent=self.root):
            return
        self.generating = True
        self.set_busy(True)
        self.status.set("Creating local test certificates… Please keep this window open until creation finishes.")

        def work():
            try:
                path = create_bundle(destination, executable)
                setup = load_setup(path)
                self.generation_results.put((True, (path, setup)))
            except Exception:
                self.generation_results.put((False, None))
        threading.Thread(target=work, daemon=True).start()

    def finish_generation(self):
        try:
            success, result = self.generation_results.get_nowait()
        except queue.Empty:
            return
        self.generating = False
        self.set_busy(False)
        if not success:
            self.status.set("Certificate creation failed. Check the selected OpenSSL executable and folder permissions. The previous client setup is unchanged.")
            return
        path, setup = result
        self.use_setup(setup, path, str(path.parent / "guest.crt"), str(path.parent / "guest.key"))
        self.status.set("Certificates created and loaded. Load this setup in the DAW: " + str(path) +
                        ". No connection was made. Copy the DAW epoch before sending.")

    def create_setup(self):
        if self.pending:
            return
        peer = simpledialog.askstring("Create local setup", "Collaborator ID (letters, numbers, _ or -):", initialvalue="guest", parent=self.root)
        if peer is None:
            return
        port = simpledialog.askinteger("Local TLS port", "Local port (1024–65535):", initialvalue=49200,
                                       minvalue=1024, maxvalue=65535, parent=self.root)
        if port is None:
            return
        paths = {}
        for field, title in (("server_cert", "Choose localhost server certificate"),
                             ("server_key", "Choose server private key"),
                             ("client_ca", "Choose trusted client CA (self-signed client: its certificate)"),
                             ("client_cert", "Choose client certificate"),
                             ("client_key", "Choose client private key")):
            path = filedialog.askopenfilename(title=title, parent=self.root)
            if not path:
                return
            paths[field] = path
        destination = filedialog.asksaveasfilename(title="Save as a NEW local setup file", parent=self.root,
                                                   defaultextension=".json", filetypes=[("JSON setup", "*.json")])
        if not destination:
            return
        try:
            setup = create_local_setup(destination, peer=peer, tls_port=port, **paths)
        except FileExistsError:
            messagebox.showerror("Choose a new filename", "Existing files are never replaced. Create setup again with a new filename.")
            return
        except (ValueError, OSError):
            messagebox.showerror("Setup not created", "Check IDs, certificate fingerprint format, matching keys, trust files and the destination folder.")
            return
        self.use_setup(setup, destination, paths["client_cert"], paths["client_key"])
        self.status.set("Setup saved and loaded. Load the same JSON in the DAW. No connection was made; live TLS verification is still required.")

    def choose_setup(self):
        path = filedialog.askopenfilename(title="Choose the DAW local test setup",
                                          filetypes=[("JSON setup", "*.json")])
        if not path:
            return
        try:
            setup = load_setup(path)
        except (OSError, ValueError, RecursionError):
            messagebox.showerror("Invalid setup", "Choose a valid local test setup with existing certificate paths.")
            return
        self.use_setup(setup, path)
        self.status.set("Setup loaded. Choose its client certificate and key, then copy the new DAW session epoch before sending.")

    def choose_credential(self, private_key):
        path = filedialog.askopenfilename(title="Choose client private key" if private_key else "Choose client certificate")
        if path:
            if private_key:
                self.client_key = path
                self.key_label.set("Client key selected (not saved)")
            else:
                self.client_cert = path
                self.cert_label.set("Client certificate selected")

    def check_setup(self):
        if self.pending:
            return
        try:
            self.status.set(check_local_setup(self.setup, self.client_cert, self.client_key))
        except (ValueError, OSError):
            self.status.set("Setup check failed. Verify the selected setup, certificate fingerprint, matching keys and PEM trust files. This test requires unencrypted keys.")

    def send(self):
        if self.pending:
            return
        if self.setup is None or not self.client_cert or not self.client_key:
            messagebox.showerror("Setup needed", "Choose the setup, client certificate and matching private key first.")
            return
        try:
            request = dict(epoch=int(self.epoch.get()), sequence=int(self.sequence.get()),
                           request_id=self.request_id.get(),
                           action={"Select track": "select", "Arm track": "arm"}[self.action.get()],
                           track_number=int(self.track.get()))
            build_request(self.setup, **request)
        except (ValueError, KeyError, OSError):
            messagebox.showerror("Check request", "Use a valid epoch, sequence, listed request ID and track number (1–128).")
            return
        # Snapshot everything on the UI thread; worker never reads Tk variables.
        setup, cert, key = dict(self.setup), self.client_cert, self.client_key
        self.set_busy(True)
        self.status.set("Sending one authenticated request…")

        def work():
            try:
                status = send_local_request(setup, cert, key, **request)
                self.results.put((True, status))
            except Exception:
                self.results.put((False, "Request not accepted. Check the active test, epoch, certificates and request ID. Stop and restart the DAW test before retrying."))
        threading.Thread(target=work, daemon=True).start()

    def poll(self):
        if self.closed:
            return
        self.finish_generation()
        try:
            success, status = self.results.get_nowait()
            self.status.set(status)
            self.set_busy(False)
            if success:
                self.sequence.set(str(int(self.sequence.get()) + 1))
        except queue.Empty:
            pass
        self.poll_id = self.root.after(100, self.poll)

    def close(self):
        if self.closed:
            return
        if self.generating:
            self.status.set("Certificate creation is still running. Please wait for it to finish before closing.")
            return
        self.closed = True
        self.root.after_cancel(self.poll_id)
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    LocalTestClient(root)
    root.mainloop()
