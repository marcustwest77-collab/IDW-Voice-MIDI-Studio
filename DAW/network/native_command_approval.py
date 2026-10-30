"""Bounded host-written approvals for exact commands on one verified connection.

This ledger does not verify TLS, grant native control, or execute a command.
Only the trusted local host controller may approve payloads. Peer messages must
never write approvals. Native pending review still requires a separate host click.
"""
import re
import threading

from command_protocol import parse_command


class NativeCommandApprovalLedger:
    capacity = 32

    def __init__(self):
        self._lock = threading.Lock()
        self._session = None
        self._highest_epoch = 0
        self._approved = {}

    @staticmethod
    def _scope(peer, epoch, connection_id):
        if (not isinstance(peer, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", peer) or
                type(epoch) is not int or not 1 <= epoch <= 2**64 - 1 or
                not isinstance(connection_id, str) or not re.fullmatch(r"[0-9a-f]{64}", connection_id)):
            raise ValueError("invalid approval session")
        return peer, epoch, connection_id

    @staticmethod
    def _command(payload, epoch):
        if type(payload) is not bytes:
            raise ValueError("immutable command bytes required")
        command = parse_command(payload)
        canonical = (f"1|{command['session_epoch']}|{command['sequence']}|{command['request_id']}|"
                     f"{command['action']}|{command['track_index']}").encode("ascii")
        if (payload != canonical or command['session_epoch'] != epoch or
                command['action'] not in ("arm", "select") or command['track_index'] >= 128):
            raise ValueError("command is not eligible for native approval")
        return command['request_id']

    def begin_session(self, peer, epoch, connection_id):
        scope = self._scope(peer, epoch, connection_id)
        with self._lock:
            if self._session is not None or epoch <= self._highest_epoch:
                return False
            self._session = scope
            self._highest_epoch = epoch
            self._approved.clear()
            return True

    def approve(self, peer, epoch, connection_id, payload):
        scope = self._scope(peer, epoch, connection_id)
        request_id = self._command(payload, epoch)
        with self._lock:
            if self._session != scope:
                return False
            if request_id in self._approved:
                return self._approved[request_id] == payload # Never replace a reviewed payload.
            if len(self._approved) >= self.capacity:
                return False
            self._approved[request_id] = payload
            return True

    def consume(self, peer, epoch, connection_id, payload):
        scope = self._scope(peer, epoch, connection_id)
        request_id = self._command(payload, epoch)
        with self._lock:
            if self._session != scope or self._approved.get(request_id) != payload:
                return False
            del self._approved[request_id]
            return True

    def revoke_session(self, peer, epoch, connection_id):
        scope = self._scope(peer, epoch, connection_id)
        with self._lock:
            if self._session != scope:
                return False # A delayed old callback cannot affect a new session.
            self._approved.clear()
            self._session = None
            return True
