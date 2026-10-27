"""Local session admission state, separate from TLS certificate authentication.

The verified_peer_identity argument MUST be supplied by a real identity provider
after authentication. It must never be copied from a guest's network payload.
"""
import re

_PEER_ID = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")


class SessionAdmission:
    def __init__(self, expected_peer_id, epoch):
        if not _PEER_ID.fullmatch(expected_peer_id) or not isinstance(epoch, int) or epoch <= 0:
            raise ValueError("invalid session identity")
        self.expected_peer_id = expected_peer_id
        self.epoch = epoch
        self.connected = False
        self.control = False
        self.recording = False

    def admit(self, verified_peer_identity, host_approved, transport_verified):
        if (not transport_verified or not host_approved or
                verified_peer_identity != self.expected_peer_id):
            return False
        self.connected = True
        return True

    def grant(self, control=False, recording=False):
        if not self.connected or (recording and not control):
            return False
        self.control = bool(control)
        self.recording = bool(recording)
        return True

    def can_request(self, peer_identity, epoch, recording=False):
        return (self.connected and self.control and
                peer_identity == self.expected_peer_id and epoch == self.epoch and
                (not recording or self.recording))

    def revoke(self):
        self.connected = False
        self.control = False
        self.recording = False
