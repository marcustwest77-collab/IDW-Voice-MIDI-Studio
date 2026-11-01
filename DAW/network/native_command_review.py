"""Bounded in-process proposal mailbox for a trusted local command reviewer.

Peer bytes can propose a command, never resolve one. This is not production IPC
or a UI, and must not be exposed as an incoming network decision endpoint.
"""
from dataclasses import dataclass
import math
import re
import secrets
import threading
import time

from native_command_approval import NativeCommandApprovalLedger
from native_tls_session import TLSConnectionIdentity


@dataclass(frozen=True)
class NativeCommandPrompt:
    generation: int
    identity: TLSConnectionIdentity
    payload: bytes
    review_owner: str # Local mailbox lifetime, not peer-supplied authority.


class NativeCommandReview:
    def __init__(self, ledger):
        if not isinstance(ledger, NativeCommandApprovalLedger): raise ValueError("host ledger required")
        self._ledger=ledger;self._changed=threading.Condition()
        self._prompt=None;self._cancel=None;self._deadline=0
        self._generation=0;self._decision=None;self._closed=False
        self._owner=secrets.token_hex(16)

    def _active(self):
        return (not self._closed and self._prompt is not None and not self._cancel.is_set() and
            time.monotonic()<self._deadline and self._ledger.is_active(*self._scope(self._prompt.identity)))

    @staticmethod
    def _scope(identity):
        return identity.peer_id,identity.epoch,identity.connection_id

    def request(self, identity, payload, *, cancel, deadline):
        """Block a transport worker, never UI/audio, until exact local decision.

        Retain the session owner's absolute command deadline (at most 5 seconds).
        Only one pending proposal is allowed. Owner must cancel/close and join
        its worker before disposing state. Session owner still consumes the
        single-use ledger approval, rechecks consent and revokes on every exit.
        """
        if (not isinstance(identity,TLSConnectionIdentity) or
                not isinstance(cancel,threading.Event) or type(deadline) not in (int,float) or
                not -1e12<=deadline<=1e12 or not math.isfinite(deadline) or
                not isinstance(identity.peer_sha256,str) or not re.fullmatch(r"[0-9a-f]{64}",identity.peer_sha256)):
            raise ValueError("invalid local command review scope")
        scope=self._scope(identity)
        # Reuse the ledger's canonical scope/command validators; approve also
        # revalidates these independently when the local decision is released.
        self._ledger._scope(*scope);self._ledger._command(payload,identity.epoch)
        if identity.tls_version not in ("TLSv1.2","TLSv1.3"): raise ValueError("verified TLS identity required")
        now=time.monotonic()
        if deadline<=now: return False
        if deadline-now>5: raise ValueError("command review deadline exceeds owner bound")
        with self._changed:
            if self._closed or self._prompt is not None or cancel.is_set() or not self._ledger.is_active(*scope): return False
            self._generation+=1;self._prompt=NativeCommandPrompt(self._generation,identity,payload,self._owner)
            self._cancel=cancel;self._deadline=deadline;self._decision=None
            try:
                while self._active() and self._decision is None:
                    self._changed.wait(min(.025,max(0,deadline-time.monotonic())))
                return (self._active() and self._decision is True and self._ledger.approve(*scope,payload))
            finally:
                self._prompt=None;self._cancel=None;self._decision=None
                self._changed.notify_all()

    def pending(self):
        with self._changed:
            return self._prompt if self._active() and self._decision is None else None

    def resolve(self, reviewed, allow):
        """Only a trusted local owner may resolve its immutable displayed snapshot."""
        if type(allow) is not bool or not isinstance(reviewed,NativeCommandPrompt): return False
        with self._changed:
            if not self._active() or self._decision is not None or reviewed!=self._prompt: return False
            self._decision=allow;self._changed.notify_all();return True

    def close(self):
        with self._changed:
            self._closed=True;self._changed.notify_all()
