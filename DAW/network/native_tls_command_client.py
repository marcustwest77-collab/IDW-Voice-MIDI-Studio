"""Explicit command exchange over a pinned client, returning queue receipts only.

One trusted local owner uses the context on one thread. No host approval is
created here. A transport receipt is not permission or evidence of DAW execution.
"""
from contextlib import contextmanager
from dataclasses import dataclass
import hmac
import math
import time

from command_protocol import parse_command
from native_tls_client import connect_native_tls_client


@dataclass(frozen=True)
class NativeQueueReceipt:
    epoch: int
    sequence: int
    request_id: str
    # Meaning: the server acknowledged queue delivery, not host approval/execution.


class _CommandClient:
    def __init__(self, secure, epoch, deadline, command_timeout, max_commands):
        self._socket=secure;self._epoch=epoch;self._deadline=deadline
        self._timeout=command_timeout;self._capacity=max_commands
        self._last_sequence=0;self._ids=set();self._closed=False

    def close(self):
        if not self._closed:
            self._closed=True;self._socket.close()

    def submit(self, payload):
        """Send one canonical select/arm request; return only its exact receipt.

        Invalid local input is rejected before I/O. Any exchange failure closes
        this connection permanently: delivery may already have happened, so no
        automatic retry is safe. Only the host can approve the queued request.
        """
        if self._closed: raise RuntimeError("native command client is closed")
        if type(payload) is not bytes: raise ValueError("immutable command bytes required")
        command=parse_command(payload)
        canonical=(f"1|{command['session_epoch']}|{command['sequence']}|{command['request_id']}|"
                   f"{command['action']}|{command['track_index']}").encode("ascii")
        if (payload!=canonical or command['session_epoch']!=self._epoch or
                command['action'] not in ("select","arm") or command['track_index']>=128 or
                command['sequence']<=self._last_sequence or command['request_id'] in self._ids or
                len(self._ids)>=self._capacity):
            raise ValueError("command is not eligible for this client session")
        until=min(self._deadline,time.monotonic()+self._timeout)
        def remaining():
            value=until-time.monotonic()
            if value<=0: raise TimeoutError("native command receipt deadline")
            return value
        def read_exact(size):
            data=bytearray()
            while len(data)<size:
                self._socket.settimeout(remaining())
                chunk=self._socket.recv(size-len(data))
                if not chunk: raise ConnectionError("native command receipt truncated")
                data.extend(chunk)
            remaining();return bytes(data)
        try:
            self._socket.settimeout(remaining())
            self._last_sequence=command['sequence'];self._ids.add(command['request_id'])
            self._socket.sendall(len(payload).to_bytes(4,"big")+payload)
            size=int.from_bytes(read_exact(4),"big")
            if not 1<=size<=4096: raise ValueError("invalid native queue receipt size")
            receipt=read_exact(size)
            if not hmac.compare_digest(receipt,payload): raise PermissionError("native queue receipt mismatch")
            return NativeQueueReceipt(self._epoch,command['sequence'],command['request_id'])
        except BaseException:
            self.close();raise


@contextmanager
def open_native_command_client(port, *, epoch, approved_server_sha256, server_ca_pem,
                               certificate_file, private_key_file, command_timeout=2.0,
                               session_timeout=30.0, max_commands=16):
    """Pinned loopback connection with bounded, explicit command submissions.

    No discovery, automatic send, consent or approval. Session time starts before
    connection establishment and cannot renew between commands. TCP/TLS uses the
    command timeout; the session deadline is checked before any command bytes.
    Never reconnect/retry an uncertain request based on this API's exceptions.
    """
    if (type(epoch) is not int or not 1<=epoch<=2**64-1 or
            type(max_commands) is not int or not 1<=max_commands<=32 or
            any(type(value) not in (int,float) or not .05<=value<=limit or not math.isfinite(value)
                for value,limit in ((command_timeout,5),(session_timeout,60)))):
        raise ValueError("invalid native command client configuration")
    deadline=time.monotonic()+session_timeout
    with connect_native_tls_client(port,approved_server_sha256=approved_server_sha256,
            server_ca_pem=server_ca_pem,certificate_file=certificate_file,
            private_key_file=private_key_file,timeout=min(command_timeout,session_timeout)) as secure:
        client=_CommandClient(secure,epoch,deadline,command_timeout,max_commands)
        try:
            if time.monotonic()>=deadline: raise TimeoutError("native command session expired during connection")
            yield client
        finally: client.close()
