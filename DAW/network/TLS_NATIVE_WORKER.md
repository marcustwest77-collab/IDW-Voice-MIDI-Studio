# Piped TLS-to-native worker (development)

The trusted Python parent can now start `TLSWorkerProcess(config)` to connect
the existing certificate-verified TLS server, `VerifiedEventProducer`, and the
native C++ local receiver. Each worker accepts exactly one TLS connection and
exits after its request succeeds or fails. Both listeners remain loopback-only.

The parent generates a fresh secret with `fresh_session_secret()` and supplies
that same secret to the native receiver and worker. Worker bootstrap uses a
bounded length-prefixed JSON message sent through an anonymous stdin pipe,
closed immediately after transmission. The token is not put in command-line
arguments, environment variables, logs, or temporary files. `close_fds=True`
limits handle inheritance. The launcher owns and terminates the child, bounds
startup time, and reports generic startup failure without configuration values.
Use a trusted installation directory for the Python scripts and executable.

Configuration must come from the local host controller, never remote input:

- `version`: integer 1.
- `native_port`, `tls_port`: distinct ports, 1024–65535.
- `token`: fresh 64-character lowercase hex secret.
- `peer`, `epoch`: approved peer ID and current positive 64-bit session epoch.
- `channel`: local session correlation value, 16–128 allowed ID characters;
  this is not a TLS exporter or a substitute for certificate verification.
- `server_cert`, `server_key`, `client_ca`: existing absolute certificate paths.
- `client_pin`: approved client certificate's lowercase SHA-256 fingerprint.
- `host_approved`, `track_control`: both must be exactly `True`.
- `approved_requests`: 1–32 distinct request IDs explicitly approved by the host.

The worker does not admit the session while parsing bootstrap. Its admission
callback runs only after the TLS server verifies the client certificate against
the configured trust, checks the pin, and maps that certificate to the approved
peer. Recording permission stays disabled. Commands still pass the one-use host
approval ledger, replay checks, producer validation, native token/peer/epoch
checks, and bounded queue insertion. TLS success means queued for host review;
the existing JUCE message-thread permission checks and approval remain required.

Host permission is a bootstrap snapshot. The controller must stop the worker
and revoke the native receiver on revocation or session end. Closing the worker
alone does not revoke already queued requests. Native receiver revocation is
tested to reject delivery even when an older worker snapshot still grants it.
If delivery fails, TLS reports failure; retries require a new approved request
and sequence. Restarting a worker does not reset native replay state.

The tests now exercise real mTLS through the child process and native C++ queue,
including wrong pins/secrets/peers, missing client certificates or request
approval, forbidden recording, replay after worker restart, revocation, receiver
shutdown, and parent-controlled worker termination. The C++ receiver fixture
stands in for the running Workstation. This is not a two-computer application
test.

Still needed: a JUCE host UI/controller that owns secret generation and pipes,
starts the guarded native receiver, launches this worker from the installed
runtime, provisions host-selected certificates and permissions, and handles
ongoing sessions. No application autostart, LAN access, remote recording, or video
is enabled by this component.
