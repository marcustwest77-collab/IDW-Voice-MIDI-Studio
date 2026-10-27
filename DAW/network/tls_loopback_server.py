"""Development-only TLS echo server bound exclusively to 127.0.0.1.

TLS protects transport to a validated hostname, but provides NO account/guest
authentication, host approval or DAW control. Never expose to LAN/Internet.
"""
import socket
import ssl
from loopback_transport import receive_frame, send_frame
from replay_guard import ReplayGuard


def serve_tls_once(port, certfile, keyfile, ready=None, *, session=None,
                   verified_peer_id=None, verified_channel_id=None, replay_guard=None,
                   client_cafile=None):
    if not (1024 <= port <= 65535) or not certfile or not keyfile:
        raise ValueError("port and TLS certificate/key are required")
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
                payload = receive_frame(secure)
                if session is not None:
                    if not verified_peer_id or not verified_channel_id:
                        raise PermissionError("trusted identity and channel binding required")
                    if replay_guard is None:
                        raise PermissionError("session replay guard required")
                    replay_guard.inspect(session, payload, verified_peer_id, verified_channel_id)
                send_frame(secure, payload)
