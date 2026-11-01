"""TEST ONLY frozen child; test configuration is baked in by its build driver.

Never distribute this fixture: session consent is automatic. The compatibility
profile also preapproves one fixed command; explicit native-review TEST mode does not.
No CLI config, environment policy, certificate discovery or production defaults.
"""
from pathlib import Path
import sys
import time

from managed_windows_tls_runtime import serve_managed_windows_tls_once
from native_command_approval import NativeCommandApprovalLedger
from native_tls_policy import build_native_server_context
from windows_bridge_launch import parse_managed_bridge_arguments
from windows_bridge_launch import hold_launch_parent
from windows_native_command_review import review_windows_native_command


def run_fixture(policy):
    launch = parse_managed_bridge_arguments(sys.argv[1:])
    context = build_native_server_context(certificate_file=policy["cert"],
        private_key_file=policy["key"], client_ca_pem=Path(policy["client_ca"]).read_text())
    ledger = NativeCommandApprovalLedger();calls = 0
    native_review=policy.get("native_review",False)
    if type(native_review) is not bool: raise ValueError("invalid TEST review mode")
    def consent(identity):
        nonlocal calls
        calls += 1
        if calls > 1 and not native_review:
            ledger.approve(identity.peer_id, identity.epoch, identity.connection_id,
                           b"1|45|1|packaged_runtime|arm|1")
        return True # Trusted TEST policy, never a production consent decision.
    def ready(port):
        if launch.ready_profile: return # Port already published on the verified private pipe.
        # Only non-secret readiness; fixed owner-selected TEST path, not IPC auth.
        destination = Path(policy["ready"])
        temporary = destination.with_suffix(".tmp")
        temporary.write_text(str(port), encoding="ascii")
        temporary.replace(destination)
    def serve(parent=None):
        def review(identity,payload,*,cancel,deadline):
            # TEST-only discovery metadata at a baked owner-selected path. It
            # contains no credential or approval and cannot replace PID/offer
            # verification. Production provisioning must use protected IPC.
            destination=Path(policy["review_endpoint"])
            while not destination.exists():
                if cancel.is_set() or time.monotonic()>=deadline: return False
                cancel.wait(.005)
            with destination.open("rb") as source: record=source.read(257)
            if len(record)>256: raise ValueError("oversized TEST endpoint metadata")
            destination.unlink()
            return review_windows_native_command(identity,payload,ledger=ledger,
                name=record.decode("ascii"),expected_server_handle=parent,cancel=cancel,deadline=deadline,timeout=3)
        options={"host_command_review":review} if native_review else {}
        return serve_managed_windows_tls_once(launch, context, expected_peer_id="guest",
            approved_client_sha256=policy["client_pin"], host_consent=consent,
            host_approval_ledger=ledger, on_listening=ready, accept_timeout=5,
            session_timeout=30, command_timeout=3,**options)
    if native_review:
        with hold_launch_parent(launch) as parent: return serve(parent)
    return serve()
