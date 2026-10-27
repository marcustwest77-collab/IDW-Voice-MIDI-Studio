"""Experimental TLS client transport with certificate and hostname verification.

Requires a separately provisioned, trusted server certificate and a server that
speaks the existing bounded framing format. This module does not grant remote
DAW permissions and is not connected to the DAW's audio or control engines.
"""
import socket
import ssl

from loopback_transport import send_frame, receive_frame


def verified_tls_client(host, port, server_name, cafile, payload, timeout=5, *, client_certfile=None, client_keyfile=None):
    if not server_name or not server_name.strip() or not cafile:
        raise ValueError("server_name and trusted CA file are required")
    if not host or not (1 <= port <= 65535):
        raise ValueError("invalid endpoint")
    if not 1 <= len(payload) <= 4096:
        raise ValueError("invalid payload")
    if bool(client_certfile) != bool(client_keyfile):
        raise ValueError("client certificate and key must be supplied together")
    context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cafile=cafile)
    if client_certfile:
        context.load_cert_chain(certfile=client_certfile, keyfile=client_keyfile)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.verify_mode = ssl.CERT_REQUIRED
    context.check_hostname = True
    with socket.create_connection((host, port), timeout=timeout) as raw:
        raw.settimeout(timeout)
        with context.wrap_socket(raw, server_hostname=server_name) as secure:
            send_frame(secure, payload)
            return receive_frame(secure)
