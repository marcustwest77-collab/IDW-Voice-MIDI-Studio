"""Trusted parent configuration for a single-request development TLS worker.

Secrets travel only through the child's anonymous stdin pipe. This is not a
remote admission API; only a local host controller may construct this snapshot.
"""
import json
from pathlib import Path
import re
import secrets
import subprocess
import sys
import queue
import threading

MAX_BOOTSTRAP = 16384
FIELDS = {"version", "native_port", "tls_port", "token", "peer", "epoch",
          "channel", "server_cert", "server_key", "client_ca", "client_pin",
          "host_approved", "track_control", "approved_requests"}


def fresh_session_secret():
    return secrets.token_hex(32)


def validate_config(config):
    if type(config) is not dict or set(config) != FIELDS:
        raise ValueError("invalid bootstrap fields")
    if type(config["version"]) is not int or config["version"] != 1:
        raise ValueError("unsupported bootstrap version")
    for field in ("native_port", "tls_port"):
        if type(config[field]) is not int or not 1024 <= config[field] <= 65535:
            raise ValueError("invalid bootstrap port")
    if config["native_port"] == config["tls_port"]:
        raise ValueError("distinct ports required")
    if type(config["epoch"]) is not int or not 0 < config["epoch"] < 2**64:
        raise ValueError("invalid bootstrap epoch")
    for field, pattern in (("token", r"[0-9a-f]{64}"), ("client_pin", r"[0-9a-f]{64}"),
                           ("peer", r"[A-Za-z0-9_-]{1,64}"),
                           ("channel", r"[A-Za-z0-9_-]{16,128}")):
        if not isinstance(config[field], str) or not re.fullmatch(pattern, config[field]):
            raise ValueError("invalid bootstrap identity")
    if config["host_approved"] is not True or config["track_control"] is not True:
        raise PermissionError("local host approval and track permission required")
    requests = config["approved_requests"]
    if type(requests) is not list or not 1 <= len(requests) <= 32:
        raise ValueError("bounded host request approvals required")
    if any(not isinstance(r, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", r)
           for r in requests) or len(set(requests)) != len(requests):
        raise ValueError("invalid host request approval")
    for field in ("server_cert", "server_key", "client_ca"):
        path = config[field]
        if (not isinstance(path, str) or len(path) > 4096 or
                not Path(path).is_absolute() or not Path(path).is_file()):
            raise ValueError("existing absolute certificate paths required")
    return config


def encode_bootstrap(config):
    validate_config(config)
    payload = json.dumps(config, separators=(",", ":"), ensure_ascii=True).encode("ascii")
    if len(payload) > MAX_BOOTSTRAP:
        raise ValueError("bootstrap too large")
    return len(payload).to_bytes(4, "big") + payload


def read_bootstrap(stream):
    def exact(size):
        chunks = bytearray()
        while len(chunks) < size:
            chunk = stream.read(size - len(chunks))
            if not chunk:
                raise ValueError("incomplete bootstrap")
            chunks.extend(chunk)
        return bytes(chunks)

    size = int.from_bytes(exact(4), "big")
    if not 0 < size <= MAX_BOOTSTRAP:
        raise ValueError("bootstrap too large or empty")

    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("duplicate bootstrap field")
            result[key] = value
        return result

    config = json.loads(exact(size).decode("utf-8"), object_pairs_hook=unique)
    # Parent closes its write handle after the one complete frame.
    if stream.read(1):
        raise ValueError("trailing bootstrap data")
    return validate_config(config)


class TLSWorkerProcess:
    """Own a bounded worker process; caller must close it on host revocation.

    No shell, arguments containing secrets, temporary secret files, or secret
    environment variables. The worker's stderr is suppressed to avoid traceback
    leakage. Startup/timeout errors deliberately contain no configuration data.
    """
    def __init__(self, config, *, startup_timeout=5.0):
        if not 0 < startup_timeout <= 10:
            raise ValueError("bounded startup timeout required")
        payload = encode_bootstrap(config)
        worker = Path(__file__).resolve().with_name("tls_native_worker.py")
        self.process = subprocess.Popen(
            [sys.executable, "-E", "-s", str(worker)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, close_fds=True,
            cwd=str(worker.parent))
        outcomes = queue.Queue(maxsize=1)

        def bootstrap():
            try:
                self.process.stdin.write(payload)
                self.process.stdin.close()
                outcomes.put(self.process.stdout.readline(64))
            except (OSError, ValueError):
                outcomes.put(b"")

        self._starter = threading.Thread(target=bootstrap, daemon=True)
        self._starter.start()
        try:
            ready = outcomes.get(timeout=startup_timeout)
            if ready != b"READY\n":
                raise RuntimeError("TLS worker startup failed")
        except (queue.Empty, RuntimeError):
            self.close()
            raise RuntimeError("TLS worker startup failed") from None

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=2)
        self._starter.join(timeout=2)
        self.process.stdin.close()
        self.process.stdout.close()

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        self.close()
