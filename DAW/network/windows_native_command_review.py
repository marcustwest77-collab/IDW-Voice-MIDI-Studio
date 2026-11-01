"""Single-use process-verified private command decision transaction, Windows only.

No discovery, launch or default approval. The raw transaction returns data only;
the explicit adapter applies verified decisions through a live local mailbox.
Use only with independently held trusted native-host identity and local policy.
"""
import ctypes
import math
import re
import threading
import time

from native_command_review_wire import encode_command_proposal, match_command_decision, MAX_DECISION_BYTES
from windows_native_bootstrap import _VerifiedPipe
from native_command_review import NativeCommandReview


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


def review_windows_native_command(identity, payload, *, ledger, name, expected_server_handle,
                                  cancel, deadline, timeout=2.0):
    """Trusted TLS callback: exact native decision -> live mailbox -> ledger.

    Invoke off UI/audio threads. Endpoint and held host identity are supplied by
    trusted local policy, never peer fields. Each invocation owns a fresh mailbox
    lifetime and worker. The pipe decision alone cannot approve a stale proposal;
    the mailbox must still be live, match exactly and belong to the active ledger.
    Always close/join the mailbox worker, including I/O, cancellation and errors.
    Session owner independently consumes approval, rechecks consent and revokes.
    No endpoint discovery, process launch, reconnect or default activation here.
    """
    review=NativeCommandReview(ledger)
    results=[];errors=[];done=threading.Event()
    def request():
        try: results.append(review.request(identity,payload,cancel=cancel,deadline=deadline))
        except BaseException as error: errors.append(error)
        finally: done.set()
    worker=threading.Thread(target=request,name="idw-private-command-mailbox")
    try: worker.start()
    except BaseException:
        review.close();raise
    try:
        prompt=None
        while not done.is_set():
            prompt=review.pending()
            if prompt is not None: break
            # request validates the scope; do not touch unvalidated cancel/time
            # inputs here. Invalid scopes finish the worker without pipe access.
            done.wait(.005)
        if prompt is None:
            if errors: raise errors[0]
            return False
        allow=receive_windows_command_decision(name,expected_server_handle,prompt,
            cancel=cancel,deadline=deadline,timeout=timeout)
        if not review.resolve(prompt,allow): return False
        worker.join()
        if errors: raise errors[0]
        return bool(results and results[0])
    finally:
        review.close();worker.join()
