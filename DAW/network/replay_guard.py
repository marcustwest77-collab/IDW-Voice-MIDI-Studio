"""Prevent replay of already accepted commands within one admitted session.

Sequence state is changed only after command parsing and permission checks pass.
This remains an isolated prototype, not a DAW dispatcher.
"""
from guarded_command_intake import inspect_request


class ReplayGuard:
    def __init__(self):
        self._last_sequence = 0
        self._request_ids = set()

    def inspect(self, session, payload, verified_peer_id, verified_channel_id):
        command = inspect_request(session, payload, verified_peer_id, verified_channel_id)
        if command["sequence"] <= self._last_sequence:
            raise PermissionError("replayed or out-of-order sequence")
        if command["request_id"] in self._request_ids:
            raise PermissionError("reused request identifier")
        self._last_sequence = command["sequence"]
        self._request_ids.add(command["request_id"])
        return command
