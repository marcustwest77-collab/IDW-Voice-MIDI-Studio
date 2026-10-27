# Remote session controller milestone

This is a **non-networked prototype** that layers an in-memory session lifecycle on top of the existing authorization and anti-replay validation in draft PRs #12 and #13. There is no TCP/WebSocket connection, camera stream, AI bridge, remote recording, or command execution.

## API
- `open(epoch)`: requires a nonzero, monotonically increasing epoch; transport/session coordinator must generate and persist its session identity.
- `admitGuest(authenticated, approved)`: must only be called after genuine authentication and separate host approval. A Jitsi meeting code **is not** authentication.
- `setPrivileges(control, recording)`: opt-in privileges; recording implies control.
- `inspect(message, state)`: runs permission, bounds, epoch and sequence checks and returns a decision. It **does not invoke DAW actions**.
- `disconnect()`: revokes the epoch, resets guest grants and replay state; stale messages cannot be authorized again through this controller.

## Required before real remote use
TLS-authenticated network transport with explicit peer identity; two-person admission and permission UI; random session IDs with durable replay protection and reconnect rules; trusted DAW-state snapshot; idempotent request IDs and acknowledgement handling; host-side user consent for recording; safe remote command dispatch with transport-thread isolation; audit and revocation tests. Session epochs in this prototype are monotonic counters, not cryptographic tokens.

Run `ctest -R IDWDAWRemote -C Release --output-on-failure` after building the branch. This code has not been compiled or hardware-tested yet.
