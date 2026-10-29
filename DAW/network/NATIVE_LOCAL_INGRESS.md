# Native authenticated local ingress (development)

`AuthenticatedLocalIngress` accepts a complete native IPC frame and delivers
only validated arm/select command data to `TrustedTransportEventQueue` through
`VerifiedLocalCommandQueueAdapter`. Workstation owns this ingress alongside its
queue; the existing JUCE message-thread timer drains events for host review.
Enqueue success means queued, not approved or executed.

`LocalSocketReceiver` now provides an explicitly started native TCP listener.
It opens no socket and starts no session by default, and provisions no secret.
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
The trusted owner must stop the receiver on transport loss, consent withdrawal,
or peer revocation. Host review still revalidates the DAW's current state.

## Opt-in loopback listener

`LocalSocketReceiver::start` accepts the same trusted admission facts and secret.
It binds only IPv4 `127.0.0.1`, defaults to an OS-assigned port, sets exclusive
address use on Windows, and does not enable address/port reuse on POSIX.
One worker handles one client at a time with a listen backlog of four. Each
connection carries exactly one native frame. The client must call write-half
shutdown after sending it and then read one byte: `1` means queued for review,
`0` means authenticated/frame intake rejected. Truncation, extra bytes, read
errors and timeouts close the connection without a success receipt. A missing
receipt does not prove a command was never queued; retries must respect replay
semantics. This is not a command execution receipt.

Reads accumulate partial headers/bodies into at most 4100 bytes. An absolute
deadline covers the whole frame and write-half shutdown, so drip-fed bytes do
not extend it. Options bound frame time (20..5000 ms), session duration (at most
five minutes), and total connection attempts (1..1024, rejected clients included).
Defaults are one second, one minute and 128 attempts. On exhaustion, expiry or
worker failure, ingress is revoked, pending commands are purged and the listener
closes. `stop` revokes the owned epoch and joins the worker before releasing
resources; polling checks shutdown every 10 ms. Call `stop` before restarting
after automatic termination. New admission needs a newer epoch and fresh secret.
Receipts are best effort, and the service can be denied by local processes
exhausting its deliberately small budgets.

Workstation owns the listener after its queue and ingress, stops it on host
revocation and destruction, and exposes only a development trusted-owner start
hook. No UI or automatic start is enabled. The existing JUCE timer still drains
the queue and revalidates/stages commands for explicit host review. Native
loopback tests exercise TCP through that queue/lifecycle/mailbox/review path;
they do not launch the DAW GUI or demonstrate a remote TLS peer.

## Remaining production work

Provision the secret over an OS-protected channel and bind admission/revocation
to the actual verified remote TLS transport. Do not accept this bearer secret
on a LAN/public listener or assume loopback establishes process identity. The
current trusted-owner booleans are preconditions, not cryptographic identity
verification. Enable a user flow only after provisioning and transport ownership
are integrated. A real two-machine TLS and host-consent demonstration remains
separate work.
