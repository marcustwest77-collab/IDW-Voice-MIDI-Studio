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

## Live TLS socket ownership and native teardown

`native_tls_session.run_native_tls_session` now owns a complete, persistent
command session on an already-handshaken server-side `ssl.SSLSocket`. It accepts
only TLS 1.2/1.3 with mandatory `CERT_REQUIRED` verification, checks the actual
peer DER certificate against the approved pin, then requests trusted host consent
and native setup. Pin/mandatory-auth/consent failures create no native lease.
The caller still supplies a trusted SSLContext/CA policy and host-owned epoch.
See [Python's TLS socket contract](https://docs.python.org/3.12/library/ssl.html).

The adapter creates a new 64-hex connection nonce after that socket's verified
handshake. It is bound to the held socket lifetime and never accepted from a
command or supplied as a pre-seeded connection ID. It is explicitly a local
connection identifier, not a TLS exporter or `tls-unique` channel-binding proof.
The native lease must match that nonce, actual peer fingerprint and epoch.
The tests now start the native receiver from those live verified values rather
than creating it with a connection ID before the TLS handshake.

The lease provider is a trusted, bounded local callback returning
`NativeTLSLease(bootstrap, revoke)`. Its revocation hook must terminate only the
captured identity/epoch; it must not globally revoke a replacement connection.
If provisioning raises before it can return a lease, that provider must roll back
internally. Every returned lease is closed exactly once, including rejection of
its bootstrap fields. A failed revocation callback is reported, never presented
as successful cleanup. Concurrent closers wait for that hook; its failure is
retained and reported to later closers without repeating the callback. This API
installs no listener or user consent UI.

One stream can carry multiple bounded arm/select commands. Host consent is
rechecked before dispatch; the existing per-command approval ledger and replay
guard still apply, and the peer certificate pin is rechecked after TLS I/O.
No permission is taken from peer-supplied identity fields. Queue receipts are
framed echoes only, not host decisions or execution results. A handoff or receipt
failure ends the session; there is no automatic retry/at-least-once guarantee.

EOF, malformed/truncated frames, replay/policy failure, command/session limits,
idle/deadline expiry and host cancellation close the bridge, revoke the native
lease and close the TLS socket. Native pending-review cleanup runs through the
lease's owner callback and the native lifecycle/message-thread path. The tests'
control fixture drains this path synchronously; they do not exercise a GUI timer.
Native host review must still process/revalidate teardown before executing work.

TLS reads/writes use nonblocking I/O with absolute per-command and whole-session
deadlines, so partial traffic cannot keep extending a session. Limits are 1..32
commands, at most 60 seconds per session and at most five seconds per command.
The local handoff inherits the remaining deadline and may only shorten it.
Cancellation is polled while waiting for TLS I/O; native handoff and trusted
callbacks have their own bounded execution contract. Host revocation should also
set the cancellation event so an idle peer is revoked without sending another
command. As elsewhere, a queue receipt can be followed by session revocation.

The live test suite uses actual pinned mTLS sockets, multiple commands on one
connection, a new nonce on reconnect, and native pending-review teardown on
close/cancel/replay/malformed frames/timeouts/budget exhaustion. It covers rejected
optional-client-auth contexts, mismatched native setup and failed revocation
hooks. All tests remain localhost and no test approves/executes a DAW action.

## Remaining production work

Managed runtime watchers now call `request_cancel()` instead of disposing the
receiver. A watcher can race the transition from readiness publication into
`serve_once`; closing a ready receiver in that window previously changed its
state to closed and caused a single-use error instead of graceful cancellation.
A cancellation request only sets the lifetime event. The serving scope observes
it and runs its finalizer; the runtime owner always closes the receiver and joins
the watcher before releasing the parent handle. Owners that request cancellation
without serving must still call `close()` to release the listener. Explicit
`close()` continues to dispose a never-started receiver and prohibit later reuse.
Regression tests force cancellation at the exact serve-entry boundary using a
real loopback receiver and a synthetic parent scope, and verify graceful return,
listener disposal and parent-scope release. Existing real native-parent/child TLS
and revocation tests remain separate coverage of the complete localhost path.

`serve_managed_windows_tls_once` now assembles the Python managed runtime. It
holds the validated actual launch parent before binding a localhost TLS listener,
keeps that handle through native lease teardown, forwards the verified TLS
identity through the private offer/transactional bootstrap, and monitors parent
lifetime plus explicit owner cancellation. Parent exit/cancellation closes the
receiver; the existing TLS finalizer clears approvals and attempts authenticated
native revocation. All listener, watcher and parent resources are closed on exit.
TLS configuration, approved peer pin, local consent and exact command approvals
remain trusted caller policy. The readiness callback receives only a non-secret
localhost port and must use trusted discovery; it cannot grant native access.
The module has no CLI or automatic startup and is not a packaged executable.
Windows integration tests now use a native C++ test host that launches an actual
Python child in an owned job. Only readiness stdout and NUL stdin are inherited;
credentials stay on the process-verified private pipe. Real parent acquisition,
mTLS, private offer, native pending review and socket revocation run together.
Tests cover EOF, payload substitution, rejected host/client pins, owner
cancellation and host exit. This test launcher is not the production pinned
`WindowsBridgeSession` launcher and uses explicit trusted test paths/configuration.
It does not demonstrate production artifact distribution, real GUI consent,
native DAW action execution, or two-machine collaboration.

`windows_bridge_launch` now parses only the fixed `--idw-native-offer`, private
pipe discovery, parent PID and epoch arguments emitted by the managed launcher.
It rejects extra arguments, noncanonical numbers, remote pipe paths and invalid
identity ranges. `hold_launch_parent` compares the argument with the actual OS
parent relationship, opens only that parent with non-inheritable query/synchronize
rights, requires it to be alive and strictly older than this process, and holds
the handle only for its context. A recycled or ambiguous creation time fails
closed. The protected pipe consumer still verifies its actual server process
against that held handle before sending an offer or reading credentials.
`receive_managed_windows_lease` composes this launch scope with the existing
transactional private offer/lease path, forwarding session cancellation/deadline.
This is a launch relationship check, not publisher trust or a secure packaging
policy. Same-user injection and privileged parent spoofing remain outside this
guard. Approved host/bridge artifacts and TLS configuration must still come from
independent trusted policy; do not treat an arbitrary parent as an approved host.
Tests exercise real Windows parent acquisition, unrelated living PID rejection,
scoped handle closure and injected creation-time ordering failure. Composition
is unit-tested; this does not claim a packaged Python bridge has been launched
by the native manager. The module adds no CLI activation or automatic listener.

`WindowsBridgeSession::startWithOffer` now manages the two-stage path. It locks
and verifies the explicitly approved executable, launches it hidden/suspended,
assigns the kill-on-close job, then resumes with fixed `--idw-native-offer`, pipe
name, host PID and planned epoch arguments. No native command listener or grant
is created before the held child supplies a private offer matching the approved
peer pin and planned epoch, and a trusted local controller returns exact-binding
host consent. The pinned helper is responsible for actual TLS verification;
the offer remains data, not cryptographic proof by itself.
Consent callbacks run synchronously on the caller's background startup thread,
must remain bounded, honor the cancellation flag, and never re-enter the manager
or retain references. Stop cancels a pending offer; after an offer is published,
matching transport loss can also interrupt a waiting consent callback. Binding
publication uses immutable atomic snapshots so stale callbacks cannot race its
initialization or affect a newer session. Expired consent creates no native
admission. A failed startup consumes its planned epoch. Failure, timeout,
denial or cancellation cleans up the owned helper/job; successful sessions use
the existing listener/process monitor and scoped native revocation.
The C++ bootstrap consumer can send an offer and validates the returned binding
before consumption. Managed process tests use synthetic TLS offers and cover
success, pin/epoch/consent rejection, late consent, stale loss, and cancellation
while waiting for offer or consent. Separate Python tests verify actual localhost
mTLS/private-offer/native-review behavior. These are separate demonstrations;
the manager has not yet launched a packaged real TLS bridge. Workstation UI and
trusted artifact distribution remain unfinished; nothing starts by default.

The private Windows channel now supports a two-stage session offer. The trusted
bridge sends a fixed 142-byte `IDWO` version-1 record (host-assigned epoch, actual
peer certificate fingerprint, fresh connection nonce and TLS 1.2/1.3 version).
Python verifies the held server process before sending even this non-secret
offer. Native `receiveOfferFrom` verifies the intended held client process before
reading it, retains a duplicated process handle, and leaves that same pipe open
for the host's policy/consent decision. Only then should the owner start native
ingress and call `deliverTo`. Delivery must match the offered epoch, fingerprint
and nonce and the still-living process; the original shared deadline also caps
host decision plus bootstrap/acknowledgment. Cancellation closes failed offers.
This data record does not verify TLS, map peers, or grant consent. The host must
trust the pinned bridge's TLS implementation, check the approved certificate and
epoch, and obtain exact-binding local consent. No remote claim can replace those
requirements. Legacy one-stage channel callers remain unchanged.
`receive_windows_native_lease(..., send_offer=True)` composes the offer with the
existing transactional bootstrap/lease path. Windows integration tests send the
actual live localhost TLS identity over the private pipe; test stdio supplies
only approved host policy and non-secret discovery, not the connection nonce or
credentials. These tests stage native pending review and revoke it through the
authenticated socket. The managed launcher and real host UI still need a
production two-stage controller and packaged TLS bridge before activation.

Native TLS lease providers now receive `(identity, *, cancel, deadline)`: the
actual session cancellation Event and absolute monotonic session deadline.
Pass both through `receive_windows_native_lease` to the protected pipe consumer.
The pipe wait/read/acknowledgment budget is the earlier of its local timeout and
the caller's deadline; partial progress never resets it. Receiver close therefore
interrupts pending Windows provisioning, including waiting for pipe discovery.
An expired consent response is checked before ledger admission/provisioning.
There is no fallback to one-argument providers that silently drop this context.
Rollback retains its separately bounded revoke timeout so an expired setup can
still attempt cleanup. This does not preempt arbitrary trusted host callbacks:
they must still honor the lifetime contract and remain bounded themselves.
Tests use real pinned localhost TLS and an actual Windows pipe discovery wait
against a held living process, plus deadline/cancellation validation. They do
not demonstrate production bridge launch or two-machine collaboration.

The live native TLS session now requires `NativeCommandApprovalLedger` instead
of the older request-ID-only prototype ledger. After verifying TLS and local host
consent, the session admits its peer, epoch and fresh connection nonce to this
ledger before requesting a native lease. Only a trusted host controller may
approve the exact immutable canonical arm/select payload. Approval covers the
sequence, request ID, action and track; a peer cannot substitute those fields
under an approved request ID. Consumption is atomic across threads. Capacity is
32 approvals, control actions/track bounds are restricted, and reviewed payloads
cannot be overwritten under the same pending ID.
Teardown clears approvals and closes the ledger session before attempting native
revocation, including when the native hook fails. Late UI approval callbacks and
stale connection callbacks are rejected; a reused ledger requires a strictly
newer epoch for a new session. A second active connection cannot rebind it.
Legacy Python protocol demonstrations retain their separate prototype ledger;
they are not used by `run_native_tls_session`. This preapproval only authorizes
delivery into native pending review. It never replaces the final local JUCE host
decision or its native transport dispatch guard. Host revocation must still
close native authority; clearing Python approvals alone does not revoke an
already-delivered native request.

`windows_native_lease.receive_windows_native_lease` now composes protected pipe
delivery and socket-backed revocation for a trusted `provide_native_lease`
callback. It takes pipe discovery and a held server process handle from the
local owner, plus identity from the verified TLS session. It constructs the
identity-bound revocable lease inside the pipe consumer, before bootstrap
acknowledgment. A failed delivery, exception or observed cancellation closes any
constructed lease before returning an error. If rollback cannot be confirmed,
`NativeLeaseRollbackError` preserves both the provisioning and rollback errors.
Successful return transfers ownership; the caller must close the lease. A cancel
racing after return is handled by the session owner's existing cancellation and
finally-close path. No PID lookup, process launch or trusted discovery policy is
provided by this helper. It does not verify TLS or grant consent by itself.
Windows integration tests use this composition with real localhost mTLS, the
C++ private pipe and authenticated native revoke, without test-stdio credentials
or test-stdio revocation. Workstation activation and bridge packaging remain
unfinished. Failure injection covers acknowledgment loss and rollback failure;
these failure injections do not claim a physical pipe fault was demonstrated.

`native_socket_lease` now supplies a native revocation hook for a protected
bootstrap and the current TLS identity. Closing the lease sends the distinct
local lifecycle record `IDW_REVOKE|1|<epoch>` inside the existing secret-bearing
frame. Native ingress checks its fresh secret and exact canonical epoch before
terminating admission, purging queued commands and admitting authentication loss.
This terminal record bypasses command backpressure but never bypasses secret or
epoch checks. It is not a remote DAW action and cannot pass command delivery.
The dispatch guard rejects the terminated epoch before acknowledgment is sent;
the receiver then closes its listener. The lease reports failed acknowledgment
instead of claiming revocation succeeded, retires its client credentials on every
attempt, and never automatically retries. An acknowledgment lost after native
termination remains an observable failure. Existing bounded listener expiry and
host/process shutdown remain independent teardown paths.
Tests exercise live localhost mTLS teardown through this authenticated revoke
path, using protected private-pipe bootstrap on Windows. EOF, cancellation,
malformed/replayed commands, consent loss, setup rejection and session budgets
clear native pending review without sending the fixture's test `REVOKE` command.
Other platform bootstrap remains a test-only stdio handoff. Packaging and trusted
host-controller activation are still outstanding.

`windows_native_bootstrap.receive_windows_bootstrap` consumes the existing C++
private named-pipe protocol from Python. The trusted caller supplies a held server
process handle (query and SYNCHRONIZE rights), plus the epoch, certificate pin
and connection nonce from its actual TLS owner. The consumer duplicates the
borrowed handle, validates the exact local pipe namespace, checks the pipe server
PID against that still-living process before reading, then validates all bootstrap
binding fields before invoking its transactional consumer. It acknowledges only
after that callback returns True within the shared deadline. Overlapped reads and
writes support cancellation and drain pending operations before freeing storage.
No PID-based process opening, network fallback, command-line secret, or automatic
activation is added. Python credential storage cannot promise a secure wipe.
If delivery or acknowledgment fails, discard captured data; the native provider
rolls back its session. The callback itself must be bounded by its trusted owner.
Non-Windows consumers fail closed. Windows tests use the actual C++ pipe provider
and live localhost mTLS/native review path; the fixture passes only non-secret pipe
discovery over its captured stdout. Other platform tests retain their existing
private test-stdio fixture and do not claim protected production provisioning.

`native_tls_receiver.NativeTLSLoopbackReceiver` now owns the explicit, single-use
IPv4 localhost accept and mTLS handshake before calling the live session owner.
Construction binds an ephemeral port; importing the module opens nothing. There
is no CLI or default workstation activation. The host supplies an exclusively
controlled TLS server context with mandatory client verification and TLS 1.2 or
newer, approved peer pin, epoch, consent, approval ledger and native lease hooks.
No native lease is requested until handshake, pin and host consent pass.
Accept and handshake each have absolute deadlines (up to ten seconds); partial
handshake progress cannot extend them. The listener closes after one accepted
connection, including rejected handshakes. Reconnection needs a fresh receiver
and host-allocated epoch. `close()` requests cancellation during accept,
handshake or the active session; the caller must join its worker before disposing
host callbacks. Active-session cancellation uses the existing native revocation
hook and clears pending native review. Certificate loading/trust policy and
bounded, transactional host callbacks remain the caller's responsibility.
Tests exercise real localhost TLS through the C++ review fixture, plus stalled
and partial handshakes, cancellation, context downgrade and single-use teardown.

Supply the real bridge executable and trusted artifact pin/distribution policy,
then integrate the live TLS session owner with its private bootstrap and trusted
host controller. The Python/native protocol and live connection-lifetime handoffs
now have localhost mTLS integration coverage; the managed launcher still exercises native
test bridge fixtures rather than a packaged TLS bridge. Implement protected provisioning for Linux/macOS before enabling
those platforms. Managed-launcher native tests use synthetic TLS-owner evidence;
separate Python integration tests verify actual localhost mTLS before private
provisioning. Windows has real child-process pipe/socket coverage, and other platforms test the codec and
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

## Managed TLS runtime policy and lifetime

Windows standalone CI additionally freezes `packaged_tls_child_fixture.py` in
PyInstaller onedir mode and drives it through `RemoteManagedTLSReviewFixture`.
The native fixture uses the actual `WindowsBridgeSession::startWithOffer` pinned
launcher, not the separate Python-interpreter test launcher. The Windows onedir
layout keeps the TLS owner in the directly launched process, preserving the
held-parent/child PID checks. The suite exercises real mTLS, private TLS offer and
bootstrap, native queue/pending review, EOF revocation, manager stop, and executable
digest rejection. Its separate synthetic smoke test is not TLS verification.
Packaging coverage runs only with explicit `IDW_PACKAGED_TLS_TESTS=1` on Windows;
missing build dependencies or fixture failures fail that CI step.

The frozen child is **test only**: automatic test consent/command approvals,
temporary certificate/key paths, and a fixed non-secret readiness file are baked
into the generated test entry. CI never uploads or installs its bundle. An exe
hash does not authenticate its dependent DLLs, Python archive or external policy
files; the test's private temporary directory supplies that ownership assumption.
A production package still needs an independently trusted whole-bundle policy,
protected credentials/configuration, discovery and actual local consent UI. This
coverage does not enable the DAW, execute track actions or demonstrate LAN/two-PC
collaboration. Do not distribute the fixture as a beta bridge.

`native_tls_policy.build_native_server_context` constructs the managed bridge's
TLS context from explicit absolute server certificate/key paths and a PEM client
CA snapshot supplied by trusted local configuration. It requires client
certificates and TLS 1.2 or newer, disables TLS tickets, and does not call system
root or environment CA discovery. It preserves the interpreter's baseline TLS
policy. Key loading never requests an interactive password; encrypted keys are
unsupported. Invalid trust, missing files and mismatched keys fail before any
listener opens. File provenance, protection against replacement during loading,
and artifact distribution remain the trusted caller's responsibility. This API
does not accept launch arguments or peer configuration as policy. Live-session
client fingerprint pinning, local consent and exact command approvals remain
mandatory separate checks. The real native-parent/Python-child integration now
uses this builder and rejects missing or unrelated client certificates before a
private native session offer. Executable packaging and host UI activation remain
unfinished.

The managed Windows runtime treats an exception from its parent-liveness monitor
as loss of authority: it requests receiver cancellation, joins the monitor and
closes the receiver before releasing the held parent handle. It reports the
monitor failure to the trusted caller. If session processing or native revocation
also fails, that error remains primary with an added monitor-failure diagnostic;
an uncertain revocation is never converted to a successful return. Deterministic
tests inject a monitor failure while serving and verify cancellation, disposal,
parent-scope release and preservation of a concurrent teardown error. This does
not add executable packaging, host UI activation or a two-computer demonstration.

## Final host dispatch boundary

The JUCE host decision handler drains queued transport facts before reading its
session grant. Final approval consumption and the short select/arm mutation also
share the transport queue mutex with admission and termination. An accepted
termination, replacement epoch, or control downgrade therefore blocks a stale UI
grant even before the UI timer drains that event. Rejected full-queue admissions
do not change this authority. UI refresh and device work happen after unlocking.
Callbacks must not re-enter the queue, wait, or perform I/O. An action already
inside the guard may finish before termination is accepted; this is an ordering
guarantee at native queue admission, not instantaneous physical disconnect
notification. Tests cover stale grants, capacity, one-shot approval, and concurrent
mutation/termination ordering using a fake adapter; they do not execute DAW tracks.
