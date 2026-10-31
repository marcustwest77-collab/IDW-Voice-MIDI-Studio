"""Bounded localhost-only client requests; no native session token is used."""
import hashlib
import ssl
import json
from pathlib import Path
import re
from tls_client import verified_tls_client

FIELDS = {"version", "peer", "tls_port", "server_cert", "server_key", "client_ca",
          "client_pin", "approved_requests"}
ID = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")


def _existing_file(path):
    return (isinstance(path, str) and 0 < len(path) <= 4096 and
            Path(path).is_absolute() and Path(path).is_file())


def validate_setup(setup):
    if type(setup) is not dict or set(setup) != FIELDS:
        raise ValueError("Choose a local test setup with only the supported connection fields.")
    if type(setup["version"]) is not int or setup["version"] != 1:
        raise ValueError("The setup version must be 1.")
    if not isinstance(setup["peer"], str) or not ID.fullmatch(setup["peer"]):
        raise ValueError("The collaborator ID is invalid.")
    if type(setup["tls_port"]) is not int or not 1024 <= setup["tls_port"] <= 65535:
        raise ValueError("The local port must be an integer from 1024 to 65535.")
    if (not isinstance(setup["client_pin"], str) or
            not re.fullmatch(r"[0-9a-f]{64}", setup["client_pin"])):
        raise ValueError("The client certificate fingerprint is invalid.")
    if any(not _existing_file(setup[field]) for field in ("server_cert", "server_key", "client_ca")):
        raise ValueError("The setup must reference existing absolute certificate/key paths.")
    requests = setup["approved_requests"]
    if (type(requests) is not list or not 1 <= len(requests) <= 32 or
            any(not isinstance(r, str) or not ID.fullmatch(r) for r in requests) or
            len(set(requests)) != len(requests)):
        raise ValueError("The setup needs 1-32 unique, valid request IDs.")
    return setup


def load_setup(path):
    with open(path, "rb") as source:
        payload = source.read(16385)
    if not 0 < len(payload) <= 16384:
        raise ValueError("The setup file must be no larger than 16 KiB.")

    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate setup fields are not allowed.")
            result[key] = value
        return result

    return validate_setup(json.loads(payload.decode("utf-8"), object_pairs_hook=unique))


def build_request(setup, *, epoch, sequence, request_id, action, track_number):
    validate_setup(setup)
    for number in (epoch, sequence):
        if type(number) is not int or not 0 < number < 2**64:
            raise ValueError("Use positive session and sequence numbers shown by your local test.")
    if not isinstance(request_id, str) or request_id not in setup["approved_requests"]:
        raise ValueError("Choose a request ID listed in the host setup.")
    if action not in ("select", "arm"):
        raise ValueError("Only track selection and arming are available.")
    if type(track_number) is not int or not 1 <= track_number <= 128:
        raise ValueError("Track number must be from 1 to 128.")
    # UI uses familiar one-based track numbers; protocol uses zero-based indexes.
    return f"1|{epoch}|{sequence}|{request_id}|{action}|{track_number-1}".encode("ascii")


def send_local_request(setup, client_cert, client_key, **request):
    payload = build_request(setup, **request)
    if not _existing_file(client_cert) or not _existing_file(client_key):
        raise ValueError("Choose your client certificate and its matching private key.")
    reply = verified_tls_client(
        "127.0.0.1", setup["tls_port"], "localhost", setup["server_cert"], payload,
        timeout=5, client_certfile=client_cert, client_keyfile=client_key)
    if reply != payload:
        raise PermissionError("The host did not acknowledge this request.")
    return "Request queued. Review it in the DAW; no track action has been approved yet."


def check_local_setup(setup, client_cert, client_key):
    """Check local material without opening sockets or prompting for key passwords."""
    validate_setup(setup)
    if not _existing_file(client_cert) or not _existing_file(client_key):
        raise ValueError("Choose the client certificate and matching private key.")
    with open(client_cert, "rb") as source:
        pem = source.read(65537)
    if len(pem) > 65536 or pem.count(b"-----BEGIN CERTIFICATE-----") != 1:
        raise ValueError("Choose one PEM client certificate, at most 64 KiB.")
    try:
        der = ssl.PEM_cert_to_DER_cert(pem.decode("ascii"))
    except (ValueError, UnicodeError):
        raise ValueError("The client certificate must be valid PEM.") from None
    if hashlib.sha256(der).hexdigest() != setup["client_pin"]:
        raise ValueError("The client certificate fingerprint does not match the setup.")
    try:
        host = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        host.load_cert_chain(setup["server_cert"], setup["server_key"], password=lambda: "")
        host.load_verify_locations(cafile=setup["client_ca"])
        client = ssl.create_default_context(cafile=setup["server_cert"])
        client.load_cert_chain(client_cert, client_key, password=lambda: "")
    except (OSError, ssl.SSLError):
        raise ValueError("Certificate/key loading failed. Check matching keys, PEM files and trust files; this test uses unencrypted keys.") from None
    return ("Local files and fingerprint checked. No connection was made. "
            "The live test still verifies certificate trust, expiry and localhost identity.")
