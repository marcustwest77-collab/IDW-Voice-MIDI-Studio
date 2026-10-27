# Experimental TLS localhost echo server

The server binds to `127.0.0.1` only, loads an externally supplied certificate and private key, and uses TLS 1.2 or newer. It exchanges a single bounded frame with a TLS client, then exits. It does **not** offer guest authentication, user accounts, peer permission validation, command parsing, DAW dispatch, or camera/audio streaming.

The tests cover fail-closed arguments, loopback bind address, certificate loading, and minimum TLS version with mocks. No actual certificate is provisioned, so **no TLS handshake or encrypted client/server round-trip is claimed yet**.

Production remote sessions need a trusted certificate and provisioning workflow, server identity verification, peer/account authentication, authorization bound to secure transport, revocation and replay handling, denial logging, cancellation, and adversarial testing. Do not expose this prototype on a public interface.

Optional `session`, `verified_peer_id`, `verified_channel_id`, and `replay_guard` inputs gate the echoed payload before it is returned. These values are **external trusted inputs only**; the TLS socket is not yet bound to a real authenticated peer identity or TLS exporter channel binding. Do not pass values extracted from untrusted network messages. No DAW action is executed.
