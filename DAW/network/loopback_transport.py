#!/usr/bin/env python3
"""Loopback-only TCP framing proof-of-concept. NO TLS, identity or remote control.

Run the server and client only on one machine. The server checks frames and echoes
the payload; it never interprets messages as DAW actions.
"""
import argparse
import socket
import struct

from command_protocol import parse_command

MAX_PAYLOAD = 4096


def read_exact(sock, size):
    data = bytearray()
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise ConnectionError("connection closed while reading")
        data.extend(chunk)
    return bytes(data)


def receive_frame(sock):
    (length,) = struct.unpack("!I", read_exact(sock, 4))
    if not 1 <= length <= MAX_PAYLOAD:
        raise ValueError("invalid frame length")
    return read_exact(sock, length)


def send_frame(sock, payload):
    if not 1 <= len(payload) <= MAX_PAYLOAD:
        raise ValueError("invalid frame length")
    sock.sendall(struct.pack("!I", len(payload)) + payload)


def serve_once(port, validate_command=False):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
        server.bind(("127.0.0.1", port))
        server.listen(1)
        server.settimeout(5)
        connection, address = server.accept()
        with connection:
            connection.settimeout(5)
            payload = receive_frame(connection)
            if validate_command:
                parse_command(payload)  # data validation only, NEVER DAW execution
            send_frame(connection, payload)
            print("echoed", len(payload), "bytes to", address[0])


def client(port, payload):
    with socket.create_connection(("127.0.0.1", port), timeout=5) as connection:
        connection.settimeout(5)
        send_frame(connection, payload)
        reply = receive_frame(connection)
        if reply != payload:
            raise RuntimeError("echo mismatch")
        print("loopback echo verified", len(reply), "bytes")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("server", "client"))
    parser.add_argument("--port", type=int, default=38941)
    parser.add_argument("--message", default="IDW loopback test")
    parser.add_argument("--validate-command", action="store_true",
                        help="require the 6-field test command grammar")
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("use a non-privileged port")
    if args.mode == "server":
        serve_once(args.port, validate_command=args.validate_command)
    else:
        client(args.port, args.message.encode("utf-8"))


if __name__ == "__main__":
    main()
