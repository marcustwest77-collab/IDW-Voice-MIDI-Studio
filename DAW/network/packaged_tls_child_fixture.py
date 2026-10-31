"""TEST ONLY frozen child; test configuration is baked in by its build driver.

Never distribute this fixture: its consent and command approvals are automatic.
No CLI config, environment policy, certificate discovery or production defaults.
"""
from pathlib import Path
import sys

from managed_windows_tls_runtime import serve_managed_windows_tls_once
from native_command_approval import NativeCommandApprovalLedger
from native_tls_policy import build_native_server_context
from windows_bridge_launch import parse_managed_bridge_arguments


def run_fixture(policy):
    launch = parse_managed_bridge_arguments(sys.argv[1:])
    context = build_native_server_context(certificate_file=policy["cert"],
        private_key_file=policy["key"], client_ca_pem=Path(policy["client_ca"]).read_text())
    ledger = NativeCommandApprovalLedger();calls = 0
    def consent(identity):
        nonlocal calls
        calls += 1
        if calls > 1:
            ledger.approve(identity.peer_id, identity.epoch, identity.connection_id,
                           b"1|45|1|packaged_runtime|arm|1")
        return True # Trusted TEST policy, never a production consent decision.
    def ready(port):
        # Only non-secret readiness; fixed owner-selected TEST path, not IPC auth.
        destination = Path(policy["ready"])
        temporary = destination.with_suffix(".tmp")
        temporary.write_text(str(port), encoding="ascii")
        temporary.replace(destination)
    return serve_managed_windows_tls_once(launch, context, expected_peer_id="guest",
        approved_client_sha256=policy["client_pin"], host_consent=consent,
        host_approval_ledger=ledger, on_listening=ready, accept_timeout=5,
        session_timeout=30, command_timeout=3)
