"""Data-only command protocol for the local network echo prototype.

No permission is inferred from wire content. This module has no DAW dispatcher.
"""
import re

_ALLOWED = {"view", "select", "arm", "record", "stop-record", "gain", "pan"}
_ID = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")


def parse_command(data: bytes):
    if not 1 <= len(data) <= 4096:
        raise ValueError("invalid payload size")
    try:
        text = data.decode("ascii")
    except UnicodeDecodeError as exc:
        raise ValueError("non-ASCII command") from exc
    fields = text.split("|")
    if len(fields) != 6:
        raise ValueError("wrong number of fields")
    version, epoch, sequence, request_id, action, track = fields
    if version != "1" or not _ID.fullmatch(request_id) or action not in _ALLOWED:
        raise ValueError("unsupported command")
    for number in (epoch, sequence, track):
        if not number or len(number) > 20 or not number.isascii() or not number.isdecimal():
            raise ValueError("invalid numeric field")
        if int(number) > 2**64 - 1:
            raise ValueError("numeric overflow")
    if int(epoch) == 0 or int(sequence) == 0:
        raise ValueError("invalid session identity")
    return {
        "session_epoch": int(epoch),
        "sequence": int(sequence),
        "request_id": request_id,
        "action": action,
        "track_index": int(track),
    }
