# Native authenticated local ingress (development)

`AuthenticatedLocalIngress` accepts a complete native IPC frame and delivers
only validated arm/select command data to `TrustedTransportEventQueue` through
`VerifiedLocalCommandQueueAdapter`. Workstation owns this ingress alongside its
queue; the existing JUCE message-thread timer drains events for host review.
Enqueue success means queued, not approved or executed.

This step opens no socket, provisions no secret, and starts no session by default.
It does not replace the Python JSON receiver protocol. The installed Windows
beta is unchanged. Live two-computer collaboration has not been demonstrated.

## Trusted owner contract

Before `beginSession`, independently verify the remote TLS peer, obtain local
host consent and track-control permission, and generate a fresh 256-bit random
secret represented by exactly 64 lowercase hexadecimal characters. Supply the
secret over an OS-protected channel accessible only to the intended local
process. Syntax checking cannot establish randomness or secure provisioning.
Admission flags and epoch must come from trusted owner state, never socket input.
The ingress does not itself verify TLS certificates or peer identity.

The native frame is a four-byte unsigned big-endian payload length (1..4096),
followed by `secret + "\n" + commandWire`. The command wire retains the six-field
`version|epoch|sequence|request-id|action|track-index` format. Secret comparison
does fixed work over all 64 bytes. Secrets are never placed in queue events or
logged; clearing a C++ string is not a guaranteed secure memory wipe.

Each complete frame is bounded and independently authenticated. Wrong secrets,
epochs, replayed sequences, unsupported actions, out-of-range tracks, truncated
frames, trailing data and oversized frames are rejected. A queue-full rejection
does not consume the sequence. Replay state belongs to the admitted session,
not an individual socket. A mutex serializes submissions with session ending.
`endSession` rejects later frames and queues authentication loss, which purges
pending commands for that epoch. Re-admission requires a strictly newer epoch.
The trusted owner must end the session on transport loss, consent withdrawal,
or peer revocation. Host review still revalidates the DAW's current state.

## Remaining listener work

Implement an explicitly enabled loopback-only listener with exclusive binding,
bounded partial-read assembly, deadlines, connection limits and shutdown/join.
Use secure OS secret provisioning and bind admission to the actual verified
remote transport. Do not accept this bearer secret on a LAN/public listener or
assume loopback establishes process identity. Exercise that listener through
the native queue and host review before enabling it in a beta build. A real
two-machine TLS and host-consent demonstration remains separate work.
