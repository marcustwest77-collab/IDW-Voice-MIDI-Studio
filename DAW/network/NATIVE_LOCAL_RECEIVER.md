# Native local receiver (development)

`RemoteNativeLocalReceiver.h` owns a background socket worker. The Workstation
owns this receiver and stops it before destruction, host revocation, session
replacement, or authentication loss. It binds only IPv4 loopback on an
OS-selected port. No receiver starts automatically.

Preparation is internal through `startNativeLocalReceiverOnMessageThread` after
explicit local host consent and permission to request track control. It requires
a fresh epoch and no already-connected collaborator, but does not mark the guest
authenticated. The caller must provide a fresh
cryptographically random 32-byte secret encoded as 64 lowercase hex characters,
the expected peer ID, and the reserved epoch. The UI stays in a waiting state,
with approvals disabled, until the TLS worker verifies the actual certificate
and sends its token-bound `CONNECT|<epoch>` message. That message queues the
connection event ahead of any command. The Windows local-test UI now calls the
Workstation process owner, which generates secrets and provisions the worker
through its private pipe. See `LOCAL_TEST_SETUP.md` and `TLS_NATIVE_WORKER.md`.
Do not put tokens
in arguments, logs, or project files.
The activation method is not exposed as a network operation.

`NativeLocalCommandBridge` is a separate native-protocol client; the earlier JSON
`LocalCommandBridge` remains compatible with its Python development receiver.
Feed the native client's `deliver` method into `VerifiedEventProducer` only after
the TLS server has verified certificates, identity, consent, and replay state.

Each connection carries one four-byte big-endian length plus an ASCII payload:
`IDWIPC1\n<token>\n<peer>\n<body>`. The body is either `CONNECT|<epoch>` or a
command wire string. The complete payload is at most 4096 bytes.
The receiver validates the fixed token, exact peer, canonical command, matching
epoch, increasing sequence, select/arm action, and track index below 128.
Numeric overflow, leading zeros, extra fields, and unsupported actions fail.
Commands before accepted connection admission fail. Duplicate CONNECT messages
are acknowledged without generating duplicate events only while that same epoch
remains admitted; ended or superseded sessions cannot be reactivated. A full
queue rejects admission without marking the session connected, permitting a
later retry. The message-thread handler retains the receiver when processing
its matching connection event instead of immediately stopping it.
It acknowledges `1` only after the bounded DAW event queue accepts the command;
`0` means rejected. Malformed or incomplete frames may close without an ACK.
An ACK means queued for review, never that a track was changed. The existing
message-thread session permission and host-review checks still apply.

The worker uses a one-second total frame deadline and 50 ms stop polling.
It processes connections sequentially and retains replay state across them.
Stopping joins the worker before enqueueing a disconnect event, clearing pending
commands for its epoch. Restart requires a fresh admitted epoch and secret.

Tests compile without JUCE. The Python integration test exercises the actual
native socket worker and queue, not the Python receiver. Windows/Linux CI runs
these tests; the Windows/macOS DAW build compiles Workstation ownership.
No end-to-end two-computer DAW test, remote recording, or video is enabled by
this change.
