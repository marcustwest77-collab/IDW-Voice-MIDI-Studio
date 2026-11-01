"""Explicit pinned localhost mTLS connection; no discovery or command sending.

Trust and client credentials come from the local owner, never readiness records.
Caller must protect credential files during loading and retain this context for
the connection lifetime. No Workstation client or automatic activation is added.
"""
from contextlib import contextmanager
import hashlib
import hmac
import math
from pathlib import Path
import re
import socket
import ssl
import time


def build_native_client_context(*, certificate_file, private_key_file, server_ca_pem):
    """Fresh explicit trust, mandatory client credentials and localhost hostname.

    No system/environment CA discovery. File integrity/provenance is the owner's
    responsibility; absolute paths alone do not establish it. Encrypted keys are
    rejected without prompting. This does not open a connection or grant consent.
    """
    if (any(not isinstance(path, (str, Path)) or not str(path) or "\0" in str(path) or
            not Path(path).is_absolute() for path in (certificate_file, private_key_file)) or
            type(server_ca_pem) is not str or not 1 <= len(server_ca_pem) <= 1024*1024 or
            not server_ca_pem.isascii() or "\0" in server_ca_pem):
        raise ValueError("explicit absolute client credentials and PEM server trust required")
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.verify_mode = ssl.CERT_REQUIRED
    context.check_hostname = True
    context.options |= ssl.OP_NO_TICKET
    context.load_verify_locations(cadata=server_ca_pem)
    def reject_password_request():
        raise ValueError("encrypted TLS private keys require a separate noninteractive owner policy")
    context.load_cert_chain(str(certificate_file), str(private_key_file), password=reject_password_request)
    return context


@contextmanager
def connect_native_tls_client(port, *, approved_server_sha256, server_ca_pem,
                              certificate_file, private_key_file, timeout=2.0):
    """Yield only a hostname/CA/pin-verified TLS socket to fixed IPv4 loopback.

    Port is discovery only. Caller independently supplies the server leaf SHA-256
    and trust policy. One absolute network budget covers TCP and TLS; no retries,
    address selection, session reuse or application bytes precede verification.
    The socket closes on every exit, including caller exceptions. After yielding,
    timeout is a per-operation socket limit, not a session-lifetime policy.
    TLS success does not imply native admission or host command approval.
    In TLS 1.3 the server's client-certificate rejection may arrive on first I/O;
    yielding proves server identity, not that the server accepted this client.
    """
    if (type(port) is not int or not 1 <= port <= 65535 or
            type(approved_server_sha256) is not str or not re.fullmatch(r"[0-9a-f]{64}", approved_server_sha256) or
            type(timeout) not in (int, float) or not .05 <= timeout <= 5 or not math.isfinite(timeout)):
        raise ValueError("invalid pinned loopback TLS configuration")
    context = build_native_client_context(certificate_file=certificate_file,
        private_key_file=private_key_file, server_ca_pem=server_ca_pem)
    deadline = time.monotonic()+timeout
    def remaining():
        value = deadline-time.monotonic()
        if value <= 0: raise TimeoutError("loopback TLS connection deadline")
        return value
    raw = secure = None
    try:
        raw = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        raw.settimeout(remaining());raw.connect(("127.0.0.1", port))
        secure = context.wrap_socket(raw, server_hostname="localhost", do_handshake_on_connect=False)
        raw = None # SSLSocket owns the descriptor, including failed handshake.
        secure.settimeout(remaining());secure.do_handshake();remaining()
        certificate = secure.getpeercert(binary_form=True)
        if (secure.version() not in ("TLSv1.2", "TLSv1.3") or not certificate or
                not hmac.compare_digest(hashlib.sha256(certificate).hexdigest(), approved_server_sha256)):
            raise PermissionError("TLS server certificate pin rejected")
        remaining();secure.settimeout(timeout)
        yield secure
    finally:
        if secure is not None: secure.close()
        if raw is not None: raw.close()
