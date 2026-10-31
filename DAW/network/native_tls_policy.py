"""Explicit TLS policy construction for the managed localhost bridge.

Inputs must come from trusted owner-selected configuration, never launch args or
peer messages. This module does not establish file provenance or artifact trust.
"""
from pathlib import Path
import ssl


def build_native_server_context(*, certificate_file, private_key_file, client_ca_pem):
    """Build a fresh mandatory-mTLS context before opening any listener.

    Absolute certificate/key paths are independently trusted caller inputs. The
    caller must protect those files against replacement while loading; resolving
    a path alone is not a file-integrity check. Client trust is an explicit PEM
    snapshot: no OS trust roots, environment CA paths or default CA discovery.
    Encrypted keys are rejected without an interactive password prompt. Client
    certificate pinning and host consent still occur in the live session owner.
    The returned context must remain exclusively host-controlled.
    """
    paths = (certificate_file, private_key_file)
    if (any(not isinstance(path, (str, Path)) or not str(path) or
            "\0" in str(path) or not Path(path).is_absolute() for path in paths) or
            type(client_ca_pem) is not str or not 1 <= len(client_ca_pem) <= 1024 * 1024 or
            "\0" in client_ca_pem or not client_ca_pem.isascii()):
        raise ValueError("explicit absolute TLS material and PEM client trust required")
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.verify_mode = ssl.CERT_REQUIRED
    # Disable tickets/resumption so a new connection authenticates a certificate
    # through the current explicit trust configuration, not an old TLS ticket.
    context.options |= ssl.OP_NO_TICKET
    context.num_tickets = 0
    context.load_verify_locations(cadata=client_ca_pem)
    def reject_password_request():
        raise ValueError("encrypted TLS private keys require a separate noninteractive owner policy")
    context.load_cert_chain(str(certificate_file), str(private_key_file), password=reject_password_request)
    return context
