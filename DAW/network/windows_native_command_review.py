"""Single-use process-verified private command decision transaction, Windows only.

No discovery/launch, ledger mutation or automatic mailbox resolution. Use only
with independently held trusted native-host process identity and local policy.
"""
import ctypes
import math
import re
import threading

from native_command_review_wire import encode_command_proposal, match_command_decision, MAX_DECISION_BYTES
from windows_native_bootstrap import _VerifiedPipe


def receive_windows_command_decision(name, expected_server_handle, prompt, *,
                                     timeout=2.0, cancel=None, deadline=None):
    """Return an exact host decision, never apply it as an approval.

    Verify the actual pipe server against a held duplicate before sending any
    proposal bytes. The borrowed source handle is never closed here. One shared
    absolute deadline covers connect, write and bounded response. Cancellation
    drains pending overlapped I/O before disposal. No reconnect or fallback.
    Caller must still resolve the live local mailbox and recheck session consent.
    """
    data=encode_command_proposal(prompt)
    if (not isinstance(name,str) or not re.fullmatch(r"\\\\\.\\pipe\\IDW-LocalCommandReview-[0-9a-f]{64}",name) or
            type(expected_server_handle) is not int or not 0<expected_server_handle<2**(8*ctypes.sizeof(ctypes.c_void_p)) or
            type(timeout) not in (int,float) or not .02<=timeout<=5 or not math.isfinite(timeout) or
            (cancel is not None and not isinstance(cancel,threading.Event)) or
            (deadline is not None and (type(deadline) not in (int,float) or
             not -1e12<=deadline<=1e12 or not math.isfinite(deadline)))):
        raise ValueError("invalid private command review configuration")
    connection=_VerifiedPipe(name,expected_server_handle,timeout,cancel,deadline)
    try:
        if not connection.transfer(bytearray(len(data).to_bytes(2,"big")+data),True):
            raise ConnectionError("private command proposal was not delivered")
        header=bytearray(2)
        if not connection.transfer(header,False): raise ConnectionError("private command decision unavailable")
        size=int.from_bytes(header,"big")
        if not 198<=size<=MAX_DECISION_BYTES: raise ValueError("invalid private command decision size")
        response=bytearray(size)
        if not connection.transfer(response,False): raise ConnectionError("private command decision truncated")
        return match_command_decision(bytes(response),prompt)
    finally:
        connection.close()
