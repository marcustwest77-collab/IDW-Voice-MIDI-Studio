# Remote Studio local test controls — Windows development build

This is one certificate-authenticated request on the same computer. It does not
connect a second computer or enable remote recording/video. Nothing starts when
the app opens. Setup and Python selection are held only in memory.

Prerequisites: a trusted Python installation, server certificate/key with
localhost identity, a trusted client CA/certificate, the approved client's
SHA-256 certificate fingerprint, and the corresponding client private key held
by the local test client. The UI does not create certificates. To avoid editing JSON, open the local test
client and choose CREATE SETUP. Enter a collaborator ID and port, then select
the server certificate/key, trusted client CA, and client certificate/key.
For a self-signed client, choose its certificate as the trusted client CA.
Choose a new JSON filename: existing files are never replaced. The client
computes the fingerprint and checks local files before saving. It then loads
the setup and selected client credentials in memory. Load that same JSON in
the DAW. Cancelling leaves your current client setup unchanged.

The saved JSON contains paths, fingerprint, collaborator ID, port and one
request ID (`local_request_1`); no private key contents or client key path is
saved. Protect the original key files. This does not install certificates or
change system trust settings. Alternatively, edit the example JSON placeholders
with your real local test paths/pin.
Do not place private key contents, passwords, session tokens, or executable paths
in the setup file. Certificate/key paths are allowed.

1. Open REMOTE REVIEW and choose LOAD TEST SETUP. Select your edited JSON file.
   Invalid types, unknown fields, missing certificate files, invalid peer/request
   IDs, and invalid fingerprints are rejected before startup.
2. Choose CHOOSE PYTHON and select python.exe or python3.exe from your trusted
   installation. The app uses its bundled remote-worker/tls_native_worker.py.
   Before starting the host, open OPEN-LOCAL-TEST-CLIENT.cmd from the extracted
   Windows package. Load the same setup, choose the client certificate and its
   matching private key, and choose a listed request ID, Select track or Arm
   track, and the visible DAW track number (starting at 1). Python must include
   tkinter. The client window keeps this information only in memory.
   Choose CHECK SETUP before starting the host. It checks local PEM loading,
   certificate/key matching and the client fingerprint without connecting.
   It does not prove trust, expiry, hostname validity or port availability;
   those still require the live TLS test. This development test uses
   unencrypted private keys, which must remain private on your computer.
3. Choose START LOCAL TEST. Review the collaborator, fingerprint and selected
   executable, then explicitly confirm. Cancel leaves the listener off.
4. Note the local session epoch displayed in the status. The TLS listener waits
   up to 60 seconds for one client. Copy that epoch into the client window and
   choose SEND REQUEST FOR HOST REVIEW. The sequence starts at 1. The client
   always connects to 127.0.0.1 with localhost hostname/certificate verification;
   it cannot target a different computer or request recording.
5. An accepted select/arm request appears for host review. Approve or deny it.
   A successful TLS reply means queued for review, not that a track changed.
6. STOP TEST or REVOKE CONTROL stops the worker/receiver and clears pending work.
   Stop before starting another test. Every new attempt reserves a fresh epoch
   and private session secret. Closing the DAW also stops the worker.

A desktop acceptance run with both windows is still required. START stays
disabled if setup, Python, or bundled worker files are missing. macOS/Linux show
the DAW's local worker controls disabled. From a source checkout, the client can
be launched with `python DAW/network/local_test_client_ui.py`.

The client reports "queued" only if the authenticated host echoes the exact
request. This never means approved or executed. On rejection, stop/restart the
host test and update the epoch before retrying. One host test accepts one TLS
connection. Closing the client does not stop the host; use STOP TEST in the DAW.
No session token is loaded or sent by this client.

## Create disposable certificates for this local test

If you do not already have test certificates, the package includes
`remote-worker/create_local_test_bundle.py`. It requires a trusted OpenSSL
executable supporting `req -addext`; it does not download or install OpenSSL.
Run this from PowerShell in the extracted Windows package, substituting the
actual executable path and a NEW folder inside your private user directory:

```powershell
python .\remote-worker\create_local_test_bundle.py --openssl "C:\Path\To\openssl.exe" --output "$env:USERPROFILE\IDW-Local-Test"
```

The helper creates fresh RSA server/client keys, self-signed certificates valid
for seven days, and a validated `local-test-setup.json` with the fingerprint.
Load that JSON in both windows; select `guest.crt` and `guest.key` in the client.
The request ID is `local_request_1`. Continue with START LOCAL TEST and copy the
DAW epoch into the client as described above. Live TLS verification still applies.

The new folder must not already exist, and its parent must exist. Failed
generation removes only that newly created folder. Nothing is installed into
Windows trust stores and no network connection is made by the helper. Private
keys are unencrypted: keep this folder private to your Windows account and do
not share/upload it. POSIX file permissions do not configure Windows ACLs.
The JSON uses absolute paths, so keep the folder in place while testing. Stop
the host test before deleting the disposable folder. Create a fresh folder when
certificates expire. These credentials are for this localhost development test,
not a remote collaborator deployment.
