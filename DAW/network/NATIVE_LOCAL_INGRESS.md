# Native authenticated local ingress (development)

`AuthenticatedLocalIngress` accepts a complete native IPC frame and delivers
only validated arm/select command data to `TrustedTransportEventQueue` through
`VerifiedLocalCommandQueueAdapter`. Workstation owns a `LocalSessionOwner` around
the ingress/listener alongside its queue; the existing JUCE message-thread timer
drains events for host review.
Enqueue success means queued, not approved or executed.

`LocalSocketReceiver` now provides an explicitly started native TCP listener.
It opens no socket and starts no session by default. Session setup generates a
secret and calls a trusted in-process handoff; no cross-process provisioning
channel is installed.
It does not replace the Python JSON receiver protocol. The installed Windows
beta is unchanged. Live two-computer collaboration has not been demonstrated.

## Trusted owner contract

Before starting the owner, independently verify the remote TLS peer and obtain
local host consent and track-control permission. `LocalTransportBinding` names
the epoch, lowercase SHA-256 peer certificate fingerprint, and fresh 64-hex
connection ID supplied by the trusted TLS implementation. `LocalHostConsent`
must name the exact same epoch, fingerprint and connection ID. Mismatches,
unverified transports and absent/revoked consent are rejected before listening.
These structs do not verify TLS; none of their fields may come from remote IPC.

`LocalSessionOwner` creates a fresh 256-bit secret using the operating system
CSPRNG: `BCryptGenRandom` on Windows and `getentropy` on Linux/macOS. It uses no
weak fallback and encodes the result as 64 lowercase hex characters. The helper
leaves its output unchanged if the OS call fails. See the
[Windows API contract](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom)
and [getentropy contract](https://man7.org/linux/man-pages/man3/getentropy.3.html).
No secret is placed in queue events, logs, command-line arguments or files.

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

Workstation owns the session owner after its queue, stops it on host revocation
and destruction, and exposes only development trusted-owner start/loss hooks.
The older Workstation local bypass methods accepting caller-supplied secrets
or command frames are removed. No UI or automatic start is enabled. The JUCE timer still drains
the queue and revalidates/stages commands for explicit host review. Native
loopback tests exercise TCP through that queue/lifecycle/mailbox/review path;
they do not launch the DAW GUI or demonstrate a remote TLS peer.

## Bootstrap and transport ownership

The start hook accepts transport evidence, matching host consent, and a required
synchronous provisioner callback. That callback receives port, epoch, secret,
peer fingerprint and connection ID as a temporary in-process bootstrap record.
It must not retain the reference, log it, send it to a remote peer, or wait
indefinitely. A process boundary requires an OS-protected channel to the intended
local bridge. The callback's presence/return value cannot establish that channel
is secure; no such channel is implemented yet.

A false/throwing handoff stops the listener and revokes its epoch. Failed
provisioning consumes the epoch, so retry needs a newer one and a fresh secret.
The callback runs without the owner mutex: host stop or transport loss can cancel
the listener during a waiting handoff. Its eventual completion cannot return
success for, or revoke, a newer session. Queued commands remain subject to host
review and revocation throughout setup. Expiry during provisioning fails closed.

`onTransportLost` stops only an owned session matching all three binding fields.
An old callback for a different epoch, peer or connection cannot terminate its
replacement. Stop is serialized with ownership changes; listener stop/join
completes before that ownership is released. After automatic expiry, explicitly
stop/clean up the owner before restarting. The external TLS implementation must
actually invoke loss/revocation hooks; identity cannot be established by the
boolean or connection ID alone.

## Remaining production work

Implement the OS-protected cross-process provisioning channel and connect the
trusted binding/consent/loss hooks to the actual verified remote TLS transport.
Current tests use synthetic TLS-owner evidence and in-process handoff only.
Do not accept this bearer secret
on a LAN/public listener or assume loopback establishes process identity. The
current trusted-owner booleans are preconditions, not cryptographic identity
verification. Enable a user flow only after provisioning and transport ownership
are integrated. A real two-machine TLS and host-consent demonstration remains
separate work.
