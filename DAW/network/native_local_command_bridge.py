"""Native C++ loopback IPC client. Provision its secret through trusted local code.

This is separate from the earlier JSON test receiver. A successful ACK means
queued for host review, not executed. It does not establish remote TLS identity.
"""
import re
import socket
from verified_event_producer import VerifiedCommandEvent


class NativeLocalCommandBridge:
    def __init__(self, port, *, session_token, timeout=2.0):
        if type(port) is not int or not 1024 <= port <= 65535:
            raise ValueError("valid loopback port required")
        if not isinstance(session_token, str) or not re.fullmatch(r"[0-9a-f]{64}", session_token):
            raise ValueError("64-character random hex secret required")
        if not 0 < timeout <= 5:
            raise ValueError("bounded timeout required")
        self.port, self.token, self.timeout = port, session_token, timeout

    def connected(self, peer_id, epoch):
        """Called by the trusted TLS worker only AFTER real certificate checks.

        Local startup must already have explicit host track-control consent.
        An ACK means the connection event was queued, not UI admission completed.
        """
        if (type(epoch) is not int or not 0 < epoch < 2**64 or
                not isinstance(peer_id, str) or
                not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", peer_id)):
            return False
        return self._send(peer_id, f"CONNECT|{epoch}")

    def deliver(self, event):
        if not isinstance(event, VerifiedCommandEvent):
            return False
        if (type(event.epoch) is not int or not 0 < event.epoch < 2**64 or
                not isinstance(event.peer_id, str) or
                not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", event.peer_id) or
                not isinstance(event.wire, str)):
            return False
        fields = event.wire.split("|")
        if len(fields) != 6 or fields[1] != str(event.epoch):
            return False
        return self._send(event.peer_id, event.wire)

    def _send(self, peer_id, wire):
        try:
            packet = f"IDWIPC1\n{self.token}\n{peer_id}\n{wire}".encode("ascii")
        except UnicodeError:
            return False
        if len(packet) > 4096:
            return False
        try:
            with socket.create_connection(("127.0.0.1", self.port), self.timeout) as connection:
                connection.settimeout(self.timeout)
                connection.sendall(len(packet).to_bytes(4, "big") + packet)
                return connection.recv(1) == b"1"
        except OSError:
            return False
