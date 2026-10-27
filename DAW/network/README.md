# Local-only TCP transport experiment

This Python script is intentionally a **network proof-of-concept**, not a DAW client or a secure collaboration service. It binds only to 127.0.0.1 and accepts **one** TCP connection, echoes a bounded (1–4096 bytes) frame, then exits. It does not parse commands, record audio, provide authentication, use TLS, connect to the Internet, or perform any DAW action.

## Local developer test
Run `python DAW/network/loopback_transport.py server` and separately `python DAW/network/loopback_transport.py client`. Run `python DAW/network/test_loopback_transport.py` for tests. No external device needed.

The true two-person studio needs independently verified peer identity; end-to-end protected signaling/media/data transport; consent and revocation; network timeouts; reconnection; command dispatch on the DAW UI thread; testing across two machines. Never expose this plain TCP echo service on public/LAN interfaces.

## Optional command grammar validation

Run `python DAW/network/loopback_transport.py server --validate-command`, then run `python DAW/network/loopback_transport.py client --message '1|25|1|request_1|arm|2'`. The server validates the command syntax before echoing it, but **does not authenticate the sender, grant permission, enqueue or execute the command**. Unsupported verbs such as `delete` are rejected. This test remains loopback-only and plaintext. It must never be exposed to the LAN or Internet.
