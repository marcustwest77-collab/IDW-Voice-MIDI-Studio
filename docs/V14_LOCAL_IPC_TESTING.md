# V14 localhost IPC integration (experimental)

This receiver is **disabled by default** and listens on IPv4 `127.0.0.1` only. It is not remote collaboration, does not expose the DAW to another computer, and does not enable video or unattended recording.

## Opt-in host setup (PowerShell, before launching a NEW development build)

```powershell
$env:IDW_LOCAL_IPC_PORT = "46219"
$env:IDW_LOCAL_IPC_EPOCH = "45"
$bytes = New-Object byte[] 32
[System.Security.Cryptography.RandomNumberGenerator]::Fill($bytes)
$env:IDW_LOCAL_IPC_TOKEN = [Convert]::ToHexString($bytes).ToLowerInvariant()
```

Keep the token private; never paste it into chat, logs, screenshots or a Git commit.
For every new session/restart, choose a greater positive epoch and a fresh token.
If PowerShell uses an older .NET without these methods, use a supported cryptographic random-number generator instead; do not substitute a guessable token.

Open the new DAW build and click **REMOTE REVIEW**. This is an explicit local host action to request a receiver start. The user interface reports that startup was requested, not necessarily that the bind succeeded. Individual commands still require host approval in Remote Review. **REVOKE CONTROL** stops the receiver. Reopening with the same epoch after revocation is intentionally rejected by the queue.

## Packet

Client connects to `127.0.0.1:$env:IDW_LOCAL_IPC_PORT`. Each TCP connection carries one big-endian 4-byte length followed by UTF-8 bytes:

```text
<64 lowercase hex token>\n1|<epoch>|<positive increasing sequence>|<request ID>|<arm or select>|<zero-based track index>
```

Maximum payload is 4096 bytes. The one-byte reply is `1` for accepted into the queue (not executed or host-approved), `0` for rejected. No reply is guaranteed for truncated packets and timeouts.

Commands reach the existing JUCE message-thread review queue, not the recording or audio callback. The server rejects incorrect tokens, noncanonical numerical fields, wrong epochs, replayed sequence numbers, out-of-range tracks and unsupported actions. Localhost access by itself is insufficient authorization.

## Boundaries

This is an experimental token-protected local bridge, **not TLS** and not a peer-authenticated two-computer receiver. The environment-secret mechanism is intended for controlled local development, not production secret provisioning. The listener cannot guarantee that a second local process with access to the host's environment is isolated. Do not grant remote clients access to it.

CI must pass the native socket CTest plus the Windows and macOS DAW foundation jobs before using the packaged build. An Actions artifact is not a signed retail release.
