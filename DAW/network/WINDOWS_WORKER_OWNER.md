# Windows worker ownership in the DAW (development)

`WindowsWorkerProcess` launches a trusted Python executable and worker script
using `CreateProcessW` with explicit application and absolute paths, without a
shell. The only inherited handles are the bootstrap stdin pipe, readiness stdout
pipe, and a NUL stderr sink. The child starts suspended, is attached to a
kill-on-close job object, then resumes. Closing the owner or revoking control
terminates the job and joins the pipe worker. Startup I/O stays off the UI thread.
The host's idle timer polls for readiness and applies a five-second startup
deadline; a blocked bootstrap write is released by terminating the child job.

The bootstrap is a four-byte length followed by at most 16 KiB of JSON, then EOF.
The process expects the exact six bytes `READY\n`. It never accepts CRLF or an
arbitrary log line as readiness. A successful child exit means delivery completed,
not that the host approved a track change. It preserves pending host reviews.
Failure or timeout stops the native receiver and ends the host session.

`startRemoteWorkerFromLocalHostOnMessageThread` now owns the sequence in the
Workstation: check local host consent, reserve a fresh epoch, generate separate
secret/correlation values with the Windows system RNG, prepare the waiting native
receiver, build the trusted bootstrap, and start the child asynchronously.
Revocation, authentication loss, and destruction stop the process owner before
the receiver. No secret is stored in arguments, environment variables, project
files, or logs. Python bootstrap validation and TLS identity checks still apply.

The controller entry point takes trusted Python/worker paths and nonsecret
settings (`peer`, `tls_port`, certificate/key paths, client pin, approved request
IDs). It is not exposed as a network operation. REMOTE REVIEW now has local-test
setup, Python selection, confirmed Start, and Stop controls. Setup selection and
permissions are not saved to projects or restored automatically. The Windows
package includes the worker scripts; the host supplies Python and certificates.
A desktop acceptance test is still required.
macOS/Linux return unsupported for this process owner; their receiver/tests remain
available. This change does not enable LAN collaboration, recording, or video.

Windows CI compiles the real Win32 owner and runs child processes to check binary
bootstrap, paths with spaces, readiness/exit handling, rejection, cancellation,
and timeout. The other standalone regression suites remain in place.
