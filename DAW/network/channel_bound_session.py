"""Bind local session admission to an externally authenticated channel identity.

The channel binding must be supplied by a trusted TLS/auth adapter, never by
remote wire data. This module is NOT an identity provider or network endpoint.
"""
import hmac
import re
from session_admission import SessionAdmission

_BINDING = re.compile(r"[A-Za-z0-9_-]{16,128}\Z")


class BoundSession(SessionAdmission):
    def __init__(self, expected_peer_id, epoch, channel_binding):
        super().__init__(expected_peer_id, epoch)
        if not isinstance(channel_binding, str) or not _BINDING.fullmatch(channel_binding):
            raise ValueError("invalid trusted channel binding")
        self._binding = channel_binding

    def admit_bound(self, verified_peer_identity, observed_channel_binding,
                    host_approved, transport_verified):
        if not isinstance(observed_channel_binding, str) or not hmac.compare_digest(
                self._binding, observed_channel_binding):
            return False
        return self.admit(verified_peer_identity, host_approved, transport_verified)

    def can_request_bound(self, peer_identity, epoch, observed_channel_binding,
                          recording=False):
        return (isinstance(observed_channel_binding, str) and
                hmac.compare_digest(self._binding, observed_channel_binding) and
                self.can_request(peer_identity, epoch, recording=recording))
