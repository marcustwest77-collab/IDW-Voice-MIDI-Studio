"""Bounded data-only command review records; no IPC or approval side effects.

Only a future held-process-verified channel may carry host decisions. Decoding
does not authenticate a sender, enforce deadlines or resolve a local mailbox.
"""
import hmac
import re

from native_command_approval import NativeCommandApprovalLedger
from native_command_review import NativeCommandPrompt
from native_tls_offer import NativeTLSOffer
from native_tls_session import TLSConnectionIdentity

MAX_PROPOSAL_BYTES=382
MAX_DECISION_BYTES=388


def encode_command_proposal(prompt):
    if (not isinstance(prompt,NativeCommandPrompt) or type(prompt.generation) is not int or
            not 1<=prompt.generation<=2**64-1 or not isinstance(prompt.review_owner,str) or
            not re.fullmatch(r"[0-9a-f]{32}",prompt.review_owner) or
            not isinstance(prompt.identity,TLSConnectionIdentity)):
        raise ValueError("invalid local command proposal")
    identity=prompt.identity
    NativeCommandApprovalLedger._scope(identity.peer_id,identity.epoch,identity.connection_id)
    NativeCommandApprovalLedger._command(prompt.payload,identity.epoch)
    if len(prompt.payload)>128: raise ValueError("command proposal payload exceeds bound")
    offer=NativeTLSOffer(identity.epoch,identity.peer_sha256,identity.connection_id,identity.tls_version).encode()
    peer=identity.peer_id.encode("ascii")
    return (b"IDWQ\x01"+prompt.generation.to_bytes(8,"big")+prompt.review_owner.encode("ascii")+
        bytes([len(peer)])+peer+offer+len(prompt.payload).to_bytes(2,"big")+prompt.payload)


def decode_command_proposal(data):
    if type(data) is not bytes or not 192<=len(data)<=MAX_PROPOSAL_BYTES or data[:5]!=b"IDWQ\x01":
        raise ValueError("invalid local command proposal record")
    peer_size=data[45]
    if not 1<=peer_size<=64 or len(data)<191+peer_size: raise ValueError("invalid proposal peer length")
    size=int.from_bytes(data[188+peer_size:190+peer_size],"big")
    if not 1<=size<=128 or len(data)!=190+peer_size+size: raise ValueError("invalid proposal command length")
    offer=data[46+peer_size:188+peer_size]
    if offer[:5]!=b"IDWO\x01" or offer[141] not in (12,13): raise ValueError("invalid proposal TLS offer")
    try:
        identity=TLSConnectionIdentity(data[46:46+peer_size].decode("ascii"),int.from_bytes(offer[5:13],"big"),
            offer[13:77].decode("ascii"),offer[77:141].decode("ascii"),"TLSv1.2" if offer[141]==12 else "TLSv1.3")
        prompt=NativeCommandPrompt(int.from_bytes(data[5:13],"big"),identity,data[190+peer_size:],data[13:45].decode("ascii"))
    except UnicodeDecodeError as error: raise ValueError("non-ASCII proposal identity") from error
    if encode_command_proposal(prompt)!=data: raise ValueError("noncanonical command proposal")
    return prompt


def encode_command_decision(prompt, allow):
    if type(allow) is not bool: raise ValueError("explicit boolean local decision required")
    return b"IDWD\x01"+bytes([int(allow)])+encode_command_proposal(prompt)


def match_command_decision(data, expected):
    """Return exact allow/deny data, never apply it to a ledger or mailbox."""
    encoded=encode_command_proposal(expected)
    if (type(data) is not bytes or not 198<=len(data)<=MAX_DECISION_BYTES or data[:5]!=b"IDWD\x01" or
            data[5] not in (0,1) or not hmac.compare_digest(data[6:],encoded)):
        raise ValueError("local command decision does not match displayed proposal")
    return data[5]==1
