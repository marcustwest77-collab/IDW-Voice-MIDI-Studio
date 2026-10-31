# Native local receiver (development)

`RemoteNativeLocalReceiver.h` owns a background socket worker. The Workstation
owns this receiver and stops it before destruction, host revocation, session
replacement, or authentication loss. It binds only IPv4 loopback on an
OS-selected port. No receiver starts automatically.

Activation is internal through `startNativeLocalReceiverOnMessageThread` after
the existing local session authority has admitted an authenticated, host-approved
collaborator with track-control permission. The caller must provide a fresh
cryptographically random 32-byte secret encoded as 64 lowercase hex characters,
the independently verified peer ID, and the admitted epoch. Secret generation,
OS-protected provisioning to the TLS process, and a launcher/UI that calls this
method remain unfinished. Do not put tokens in arguments, logs, or project files.
The activation method is not exposed as a network operation.

`NativeLocalCommandBridge` is a separate native-protocol client; the earlier JSON
`LocalCommandBridge` remains compatible with its Python development receiver.
Feed the native client's `deliver` method into `VerifiedEventProducer` only after
the TLS server has verified certificates, identity, consent, and replay state.

Each connection carries one four-byte big-endian length plus an ASCII payload:
`IDWIPC1\n<token>\n<peer>\n<wire>`. The complete payload is at most 4096 bytes.
The receiver validates the fixed token, exact peer, canonical command, matching
epoch, increasing sequence, select/arm action, and track index below 128.
Numeric overflow, leading zeros, extra fields, and unsupported actions fail.
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
