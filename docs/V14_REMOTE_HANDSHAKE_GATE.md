# V14 two-person handshake gate (experimental)

This layer **does not authenticate** peers. It accepts only externally verified claims from a future secure transport: encrypted channel established, cryptographic identity verified and host explicitly approved the displayed peer identity. A peer's assertions inside an untrusted network message must never be used as these booleans.

The gate checks protocol version, bounded peer identifier, current session epoch, verified transport assertion, verified identity assertion and explicit host consent before the existing session controller admits a guest.

No sockets, TLS handshake, password storage, peer certificates, audio/video transfer or remote DAW control exist in this patch. An invitation link or Jitsi room is **not** a login.

Production prerequisites: verified authenticated transport integration, consent prompts bound to identity and session, denial logging without leaking secrets, exactly-two-peer admission, replay and reconnect safeguards, tests with a real network adapter, secure credential storage, and adversarial security review.

This code is for local unit tests and must not be exposed as remote authorization by itself.
