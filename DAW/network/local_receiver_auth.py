"""Test-only authenticated localhost receiver contract, not a DAW socket listener.

A separate per-session random secret is required; loopback address alone is
not authentication. Production must use OS-protected secret provisioning,
bind session consent to the verified TLS peer, and deliver only to the JUCE
message-thread event queue after independent verification.
"""
import hmac
import json
import secrets


def reject_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate IPC field")
        result[key] = value
    return result


class LocalReceiverAuthenticator:
    def __init__(self, token: str, peer_id: str, epoch: int):
        if (not isinstance(token, str) or len(token) < 32 or
                not peer_id or type(epoch) is not int or epoch <= 0):
            raise ValueError("random session secret, peer, epoch required")
        self._token = token
        self._peer = peer_id
        self._epoch = epoch
        self._highest_sequence = 0

    @staticmethod
    def fresh_secret():
        return secrets.token_hex(32)

    def accept(self, packet: bytes):
        if not isinstance(packet, bytes) or len(packet) > 4096:
            return None
        try:
            payload = json.loads(packet.decode("utf-8"), object_pairs_hook=reject_duplicate_keys)
        except (UnicodeError, ValueError):
            return None
        if not isinstance(payload, dict) or set(payload) != {
            "type", "epoch", "peer", "wire", "token"
        }:
            return None
        token = payload["token"]
        if not isinstance(token, str) or not hmac.compare_digest(token, self._token):
            return None
        if (payload["type"] != "command" or
                type(payload["epoch"]) is not int or payload["epoch"] != self._epoch or
                payload["peer"] != self._peer or not isinstance(payload["wire"], str)):
            return None
        fields = payload["wire"].split("|")
        if len(fields) != 6 or fields[0] != "1" or fields[1] != str(self._epoch):
            return None
        if (not fields[2].isdigit() or not fields[5].isdigit() or
                fields[4] not in ("arm", "select") or
                not fields[3] or len(fields[3]) > 128):
            return None
        sequence, track = int(fields[2]), int(fields[5])
        if (sequence <= self._highest_sequence or track >= 128 or
                fields[2] != str(sequence) or fields[5] != str(track)):
            return None
        self._highest_sequence = sequence
        return payload["wire"]
