# Remote Studio local test controls — Windows development build

This is one certificate-authenticated request on the same computer. It does not
connect a second computer or enable remote recording/video. Nothing starts when
the app opens. Setup and Python selection are held only in memory.

Prerequisites: a trusted Python installation, server certificate/key with
localhost identity, a trusted client CA/certificate, the approved client's
SHA-256 certificate fingerprint, and the corresponding client private key held
by the local test client. The UI does not create certificates. The example JSON
contains placeholders and must be edited with your real local test paths/pin.
Do not place private key contents, passwords, session tokens, or executable paths
in the setup file. Certificate/key paths are allowed.

1. Open REMOTE REVIEW and choose LOAD TEST SETUP. Select your edited JSON file.
   Invalid types, unknown fields, missing certificate files, invalid peer/request
   IDs, and invalid fingerprints are rejected before startup.
2. Choose CHOOSE PYTHON and select python.exe or python3.exe from your trusted
   installation. The app uses its bundled remote-worker/tls_native_worker.py.
3. Choose START LOCAL TEST. Review the collaborator, fingerprint and selected
   executable, then explicitly confirm. Cancel leaves the listener off.
4. Note the local session epoch displayed in the status. The TLS listener waits
   up to 60 seconds for one client. The client must use that epoch, an increasing
   sequence, an approved request ID, and the matching client certificate/key.
5. An accepted select/arm request appears for host review. Approve or deny it.
   A successful TLS reply means queued for review, not that a track changed.
6. STOP TEST or REVOKE CONTROL stops the worker/receiver and clears pending work.
   Stop before starting another test. Every new attempt reserves a fresh epoch
   and private session secret. Closing the DAW also stops the worker.

There is no bundled end-user client UI yet; the existing Python TLS client is a
developer test component. A desktop acceptance run with these controls is still
required. START stays disabled if setup, Python, or bundled worker files are
missing. macOS/Linux show these local worker controls disabled.
