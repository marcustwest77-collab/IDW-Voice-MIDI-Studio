"""Development-only one-shot authenticated localhost command receiver.

Independent of JUCE; validates a per-session secret and forwards accepted wire
data to a supplied callback. This must not be exposed on LAN/Internet, and a
production DAW must provision the secret securely and verify local host consent.
"""
import socket
from local_receiver_auth import LocalReceiverAuthenticator


def serve_local_once(port, authenticator, accepted_wire_sink, ready=None):
    if type(port) is not int or not 1024 <= port <= 65535:
        raise ValueError("valid port required")
    if not isinstance(authenticator, LocalReceiverAuthenticator):
        raise ValueError("receiver authenticator required")
    if not callable(accepted_wire_sink):
        raise ValueError("accepted wire callback required")
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", port))
        listener.listen(1)
        listener.settimeout(5)
        if ready is not None:
            ready.set()
        connection, _ = listener.accept()
        with connection:
            connection.settimeout(2)
            def read_exact(count):
                data = b""
                while len(data) < count:
                    chunk = connection.recv(count - len(data))
                    if not chunk:
                        raise ConnectionError("short IPC frame")
                    data += chunk
                return data
            try:
                length = int.from_bytes(read_exact(4), "big")
                if not 0 < length <= 4096:
                    raise ValueError("oversize or empty IPC frame")
                wire = authenticator.accept(read_exact(length))
                if wire is None or accepted_wire_sink(wire) is not True:
                    connection.sendall(b"0")
                    return False
                connection.sendall(b"1")
                return True
            except (OSError, ValueError, ConnectionError):
                return False
