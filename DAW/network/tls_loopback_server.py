"""Development-only TLS echo server bound exclusively to 127.0.0.1.

TLS protects transport to a validated hostname, but provides NO account/guest
authentication, host approval or DAW control. Never expose to LAN/Internet.
"""
import socket
import hashlib
import hmac
import ssl
from loopback_transport import receive_frame, send_frame
from replay_guard import ReplayGuard


def serve_tls_once(port, certfile, keyfile, ready=None, *, session=None,
                   verified_peer_id=None, verified_channel_id=None, replay_guard=None,
                   client_cafile=None, approved_client_sha256=None,
                   approved_peer_by_sha256=None):
    if not (1024 <= port <= 65535) or not certfile or not keyfile:
        raise ValueError("port and TLS certificate/key are required")
    if approved_client_sha256 is not None:
        if not client_cafile or len(approved_client_sha256) != 64 or any(
            ch not in '0123456789abcdef' for ch in approved_client_sha256
        ):
            raise ValueError('client trust and lowercase SHA-256 fingerprint required')
    if approved_peer_by_sha256 is not None:
        if not isinstance(approved_peer_by_sha256, dict) or not approved_peer_by_sha256:
            raise ValueError('trusted certificate-to-peer mapping required')
        if not client_cafile:
            raise ValueError('certificate mapping requires mutual TLS')
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(certfile=certfile, keyfile=keyfile)
    if client_cafile:
        context.load_verify_locations(cafile=client_cafile)
        context.verify_mode = ssl.CERT_REQUIRED
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", port))
        listener.listen(1)
        listener.settimeout(5)
        if ready is not None:
            ready.set()
        raw, _ = listener.accept()
        with raw:
            raw.settimeout(5)
            with context.wrap_socket(raw, server_side=True) as secure:
                if client_cafile and not secure.getpeercert(binary_form=True):
                    raise PermissionError("verified client certificate required")
                if approved_client_sha256 is not None:
                    peer_certificate = secure.getpeercert(binary_form=True)
                    if not peer_certificate or not hmac.compare_digest(
                        hashlib.sha256(peer_certificate).hexdigest(), approved_client_sha256
                    ):
                        raise PermissionError('unapproved client certificate')
                # A protected session must be backed by mandatory mutual TLS and pinning.
                # External peer/channel assertions alone are not sufficient.
                if session is not None and (not client_cafile or not approved_client_sha256):
                    raise PermissionError('protected sessions require pinned mTLS identity')
                certificate_peer = None
                if approved_peer_by_sha256 is not None:
                    peer_der = secure.getpeercert(binary_form=True)
                    if not peer_der:
                        raise PermissionError('peer certificate required')
                    certificate_peer = approved_peer_by_sha256.get(hashlib.sha256(peer_der).hexdigest())
                    if not certificate_peer:
                        raise PermissionError('client certificate not mapped to approved guest')
                payload = receive_frame(secure)
                if session is not None:
                    if approved_peer_by_sha256 is None or certificate_peer != session.expected_peer_id:
                        raise PermissionError('certificate identity does not match approved session guest')
                    if not verified_channel_id:
                        raise PermissionError("trusted identity and channel binding required")
                    if replay_guard is None:
                        raise PermissionError("session replay guard required")
                    replay_guard.inspect(session, payload, certificate_peer, verified_channel_id)
                send_frame(secure, payload)
