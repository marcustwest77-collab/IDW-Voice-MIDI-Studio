"""Validated, admission-gated command intake for the development session model.

The caller supplies an identity and channel binding obtained from trusted
authentication/transport components, never from the untrusted command body.
This does not authorize or execute commands in the actual DAW.
"""
from command_protocol import parse_command


def inspect_request(session, payload, verified_peer_id, verified_channel_id):
    command = parse_command(payload)
    if not session.can_request_bound(
        verified_peer_id,
        command["session_epoch"],
        verified_channel_id,
        recording=command["action"] in {"record", "stop-record"},
    ):
        raise PermissionError("session grant or channel binding invalid")
    return command
