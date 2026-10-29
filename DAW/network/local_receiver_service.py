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


def serve_local_session(port, authenticator, accepted_wire_sink, ready=None,
                        *, max_connections=8):
    """Bounded development receiver: retain replay state across connections.

    No JUCE integration or secure OS secret provisioning; test-only.
    A rejected connection consumes one attempt. The listener terminates after
    max_connections attempts, including malformed or unauthorized attempts.
    """
    if type(max_connections) is not int or not 1 <= max_connections <= 32:
        raise ValueError("bounded connection count required")
    if type(port) is not int or not 1024 <= port <= 65535:
        raise ValueError("valid port required")
    if not isinstance(authenticator, LocalReceiverAuthenticator):
        raise ValueError("receiver authenticator required")
    if not callable(accepted_wire_sink):
        raise ValueError("accepted wire callback required")
    accepted = 0
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", port))
        listener.listen(1)
        listener.settimeout(5)
        if ready is not None:
            ready.set()
        for _ in range(max_connections):
            connection, _ = listener.accept()
            with connection:
                connection.settimeout(2)
                try:
                    def read_exact(length):
                        result = b""
                        while len(result) < length:
                            chunk = connection.recv(length - len(result))
                            if not chunk:
                                raise ConnectionError("short frame")
                            result += chunk
                        return result
                    size = int.from_bytes(read_exact(4), "big")
                    if size < 1 or size > 4096:
                        raise ValueError("invalid frame length")
                    wire = authenticator.accept(read_exact(size))
                    if wire is not None and accepted_wire_sink(wire) is True:
                        accepted += 1
                        connection.sendall(b"1")
                    else:
                        connection.sendall(b"0")
                except (OSError, ValueError, ConnectionError):
                    pass
    return accepted
