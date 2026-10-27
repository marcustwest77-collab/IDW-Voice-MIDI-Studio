# Experimental TLS client foundation

This module is a certificate-validating **client** for the existing bounded frame protocol. It requires a hostname and CA certificate supplied by a trusted provisioning process; it enforces hostname checking, certificate verification and TLS 1.2 or newer. It never disables verification.

No server implementation, certificate provisioning, peer account authentication, identity onboarding, DAW dispatcher, video connection, or live cross-computer session exists yet. A valid TLS server certificate does not alone prove that the remote user has authorization to operate DAW controls. Do not use this module with credentials or session secrets until server identity and account authorization are designed and security-reviewed.

Run `python DAW/network/test_tls_client.py` for isolated configuration tests. TLS negotiation and cross-machine functionality are not yet integration-tested.
