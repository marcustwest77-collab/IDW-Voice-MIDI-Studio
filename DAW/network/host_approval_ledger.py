"""Host-owned per-command approval ledger for development-only mTLS pathway.

Approvals must be written by a trusted host UI/controller, NEVER by a peer
message. Approved requests are single use and bound to peer, epoch and ID.
"""
class HostApprovalLedger:
    def __init__(self):
        self._approved = set()

    def approve(self, peer, epoch, request_id):
        if not isinstance(peer, str) or not peer or not isinstance(epoch, int) or epoch <= 0:
            raise ValueError("invalid approval identity")
        if not isinstance(request_id, str) or not request_id:
            raise ValueError("invalid approval request")
        self._approved.add((peer, epoch, request_id))

    def contains(self, peer, epoch, request_id):
        return (peer, epoch, request_id) in self._approved

    def consume(self, peer, epoch, request_id):
        key = (peer, epoch, request_id)
        if key not in self._approved:
            return False
        self._approved.remove(key)
        return True

    def revoke_peer(self, peer):
        self._approved = {key for key in self._approved if key[0] != peer}
