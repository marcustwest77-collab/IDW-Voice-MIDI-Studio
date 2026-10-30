# Native authenticated local ingress (development)

`AuthenticatedLocalIngress` accepts a complete native IPC frame and delivers
only validated arm/select command data to `TrustedTransportEventQueue` through
`VerifiedLocalCommandQueueAdapter`. Workstation owns a `LocalSessionOwner` around
the ingress/listener through a `WindowsBridgeSession` alongside its queue; the JUCE timer
drains events for host review.
Enqueue success means queued, not approved or executed.

`LocalSocketReceiver` now provides an explicitly started native TCP listener.
It opens no socket and starts no session by default. Session setup generates a
secret and calls a trusted provisioner. An opt-in Windows private pipe channel
is available for a host-selected bridge process; no bridge is launched by default.
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
CSPRNG: `BCryptGenRandom` on Windows, `SecRandomCopyBytes` from Security.framework
on macOS, and `getentropy` on Linux. It uses no
weak fallback and encodes the result as 64 lowercase hex characters. The helper
leaves its output unchanged if the OS call fails. See the
[Windows API contract](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom)
the [Apple API contract](https://developer.apple.com/documentation/security/secrandomcopybytes(_:_:_:)),
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

Workstation owns the bridge/session manager after its queue, stops it on host revocation
and destruction, and exposes only development trusted-owner start/loss hooks.
The older Workstation local bypass methods accepting caller-supplied secrets
or command frames are removed. No UI or automatic start is enabled. The JUCE timer still drains
the queue and revalidates/stages commands for explicit host review. Native
loopback tests exercise TCP through that queue/lifecycle/mailbox/review path;
they do not launch the DAW GUI or demonstrate a remote TLS peer.

## Bootstrap and transport ownership

The lower-level `LocalSessionOwner::start` accepts transport evidence, matching
host consent, and a required synchronous provisioner callback. That callback receives port, epoch, secret,
peer fingerprint and connection ID as a temporary in-process bootstrap record.
It must not retain the reference, log it, send it to a remote peer, or wait
indefinitely. A process boundary requires an OS-protected channel to the intended
local bridge. On Windows, `WindowsBootstrapChannel` supplies that channel when
the host provides trusted held process handles. A callback alone does not prove
that its chosen channel or process is trusted.

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

## Windows private bootstrap channel

`WindowsBootstrapChannel::create` creates a single-instance, overlapped named
pipe under `\\.\pipe\IDW-LocalBootstrap-` with a fresh random discovery suffix.
The discovery name is not a credential. An explicit protected DACL grants access
only to the current user SID; `PIPE_REJECT_REMOTE_CLIENTS` rejects remote clients
and `FILE_FLAG_FIRST_PIPE_INSTANCE` rejects a pre-existing instance. No pipe
handles are inherited. The host creates the endpoint before giving its name to
the intended bridge. The session secret is never included in that name, argv,
environment variables, logs or temporary files by this implementation.

The host calls `deliverTo` with a held process handle for its intended bridge,
not a PID supplied by the bridge. The handle must have query and SYNCHRONIZE
rights. Before sending any secret bytes, the server compares Windows'
`GetNamedPipeClientProcessId` result to that live process handle. It disconnects
wrong clients without data and permits at most eight connection attempts.
Holding the process handle through transfer also avoids relying on a recycled
numeric PID. On the receiving side, `receiveWindowsBootstrap` checks
`GetNamedPipeServerProcessId` against a trusted held host process handle before
reading. Client SQOS limits the server to identification, not impersonation.
See [Windows pipe creation](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createnamedpipew),
[client process identity](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getnamedpipeclientprocessid),
and [server process identity](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getnamedpipeserverprocessid).

The fixed 207-byte binary record carries a magic/version, port, epoch and exactly
64 lowercase hex bytes each for secret, peer fingerprint and connection ID.
The consumer must validate/use the binding and return true before a fixed receipt
is sent. This receipt acknowledges bootstrap consumption only. Failed/throwing
consumers, bad records, missing receipts, peer exit, timeout or cancellation fail
the provisioner; the session owner then revokes the listener and queued work.
Each server instance is consumed on every delivery outcome. I/O uses a shared
absolute deadline of 20..5000 ms (default two seconds) and checks cancellation
and peer liveness while waiting. Pending I/O is canceled and drained before
buffers/handles are released. A trusted consumer callback itself must be bounded.

The channel plugs into the existing `LocalSessionOwner::Provisioner` by calling
`channel.deliverTo(heldBridgeProcess, bootstrap)` in that callback. Host transport
loss should stop the session owner and call `channel.cancel` if a handoff is still
waiting. Code on other platforms fails closed; private provisioning there is not
implemented. Windows tests launch a real child with no inherited handles, deliver
a generated secret over this pipe, and submit its TCP command to the native queue.
They also cover wrong client/server identity, rejected/throwing consumers,
one-shot consumption, timeout and cancellation. These are local tests with
synthetic TLS-owner evidence, not a two-machine demonstration.

The host is still responsible for choosing and verifying the actual bridge
executable and its process handles. This boundary does not defend against an
administrator/debugger or a process already able to inject into/read the trusted
host/bridge. It also cannot prevent a local process from denying service by
consuming connection budgets. Do not enable it with untrusted process handles.

## Managed Windows bridge launch

Workstation's development start hook now accepts an explicitly selected absolute
executable path and a separately approved lowercase SHA-256 pin, plus matching
transport evidence/host consent. It uses `WindowsBridgeSession` rather than an
arbitrary caller-supplied provisioner. No executable is selected or launched by
default, and there is no UI activation. Linux/macOS fail closed.

The launcher requires a local fixed-drive path and regular non-reparse file,
opens it without write/delete sharing, computes SHA-256 with BCrypt, and checks
the supplied pin. It launches the canonical `.exe` path explicitly, with its
directory as the working directory, without a shell or PATH search. The image
file handle stays locked through process creation and bootstrap. The only
arguments are `--idw-native-bootstrap`, the non-secret pipe name, and host PID.
No handles are inherited. The console is hidden.
Launch-time OS error dialogs are suppressed with a temporary thread-local error
mode, then the prior mode is restored, so an invalid image returns failure.

The pin must come from a trusted manifest/policy. The diagnostic
`inspectWindowsBridgeSha256` merely computes a digest; automatically trusting its
result for an untrusted file would defeat pinning. This step is a file hash check,
not publisher signature verification or a complete dependency integrity policy.
The installation directory and the bridge's dependencies must also be trusted.

The process starts suspended, is assigned to an unnamed kill-on-close job, then
resumes and consumes its secret through the private pipe. Assignment/resume or
bootstrap failure revokes the listener and cleans up the owned child. A monitor
checks the child and listener lifetime: child exit, wait failure or receiver
expiry revokes the matching epoch and terminates the owned job. Host stop/loss,
revocation and destruction cancel pending bootstrap, join the monitor and stop
the bridge and descendants before releasing handles. These controls only act
on processes created by this manager; no arbitrary PID termination API is exposed.
See [Windows job objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects)
and [suspended process creation](https://learn.microsoft.com/en-us/windows/win32/procthread/process-creation-flags).

Each startup has a separate cancellation/channel record. Transport-loss handling
can revoke a waiting bootstrap without waiting for the launch mutex, and a stale
record cannot cancel a replacement. The existing owner epoch/identity checks
still apply. After automatic termination, call `stop` to release the old monitor
and handles before restarting. Cleanup uses forced termination: the bridge is
a disposable transport helper, never an owner of DAW recording/project data.

Windows tests pin and launch a real test bridge, transfer its generated secret,
and authenticate its TCP command into the native queue. They check a known
SHA-256 vector, pin/path/consent rejection, correctly pinned invalid-image rollback, bridge exit, receiver expiry,
bootstrap timeout, stop/loss during startup, stale loss, restart/destruction and
termination of a real descendant process. These fixtures do not implement TLS.

## Verified Python TLS sink to native review

`native_local_bridge.py` adapts the existing `VerifiedEventProducer` sink to the
native receiver's protocol, separately from the older JSON IPC prototype.
`NativeLocalBootstrap.decode` accepts only the exact 207-byte native record and
validates every field. Its representation redacts credentials. Constructing the
bridge requires the expected peer name plus the trusted TLS owner's certificate
fingerprint and connection ID, both matching that bootstrap. These supplied
strings remain preconditions, not a substitute for TLS verification.

Each event must name that peer/epoch and contain a canonical, bounded arm/select
wire command with a valid track index. The client sends the native secret/newline
envelope to `127.0.0.1` only, then performs write-half shutdown before reading the
native queue receipt. One absolute deadline covers connect/send/shutdown/receipt.
Replay authority remains native; a failed delivery does not invent a new sequence
or approve a command. `close` disables further submissions but cannot securely
wipe Python string copies. A success receipt is queue admission only; fresh DAW
state and explicit host review still govern the next stage.

The client does not authenticate the process owning the TCP receiver port. Use
it only with protected bootstrap and host-controlled bridge/receiver lifetimes;
it is not a production replacement for the managed Windows bridge. It starts no
listener, grants no permission and logs no credentials.

The new three-platform integration runs a certificate-verified/pinned localhost
mTLS server with peer mapping, host approval ledger and replay guard, through
`VerifiedEventProducer` and this client into the actual C++ localhost receiver,
trusted queue, lifecycle/mailbox and pending host-review coordinator. Tests reject
missing client certificates, wrong pins/peer mappings, denied/missing approval,
recording actions, malformed messages, replay and stale post-revocation commands.
A busy DAW rejects arming at native review even when the queue receipt succeeded.

`RemoteNativeLocalReviewFixture` is test-only. It refuses terminal/file stdio,
emits bootstrap bytes only to a captured parent pipe, and offers bounded local
status/revocation controls. Tests use throwaway certificates and real TLS
handshakes, but the fixture's initial native admission and connection ID are
synthetic trusted setup. Its parent stdio is not the Windows private pipe protocol.
The tests do not launch the JUCE GUI or exercise a packaged managed TLS bridge,
and no test approves or executes DAW actions. They are all on one computer.

## Remaining production work

Supply the real bridge executable and trusted artifact pin/distribution policy,
then connect its private bootstrap and trusted binding/consent/loss hooks to the
actual verified remote TLS transport. The Python/native protocol handoff now has
localhost mTLS integration coverage; the managed launcher still exercises native
test bridge fixtures rather than a packaged TLS bridge. Implement protected provisioning for Linux/macOS before enabling
those platforms. Current tests use synthetic TLS-owner evidence; Windows has
real child-process pipe/socket coverage, and other platforms test the codec and
fail-closed unsupported path.
The standalone native CI runs on Windows, Linux and macOS, including the real
socket/session-owner tests. Foundation builds also compile/link the workstation
and its session-owner test with the platform entropy library.
Do not accept this bearer secret
on a LAN/public listener or assume loopback establishes process identity. The
current trusted-owner booleans are preconditions, not cryptographic identity
verification. Enable a user flow only after provisioning and transport ownership
are integrated. A real two-machine TLS and host-consent demonstration remains
separate work.
