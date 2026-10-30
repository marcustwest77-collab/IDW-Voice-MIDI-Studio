"""Explicit managed Windows TLS runtime. No CLI, artifact selection or auto start.

The trusted caller supplies TLS/artifact policy, local consent and host-written
command approvals. This assembles existing components; it is not a packaged exe.
"""
import threading
import math
import re

import windows_bridge_launch as launch_api
from native_command_approval import NativeCommandApprovalLedger
from native_tls_receiver import NativeTLSLoopbackReceiver
from windows_native_lease import receive_windows_native_lease


def serve_managed_windows_tls_once(launch, context, *, expected_peer_id, approved_client_sha256,
                                   host_consent, host_approval_ledger, on_listening,
                                   cancel=None, accept_timeout=5.0, handshake_timeout=2.0,
                                   session_timeout=30.0, command_timeout=2.0, max_commands=16):
    """Hold the verified launch parent before binding, and through all teardown.

    on_listening receives a non-secret localhost port and must be bounded. It
    must publish only via trusted owner-selected discovery. This callback does
    not grant control. Host consent and exact command approval remain separate.
    Cancellation/parent exit closes the receiver; native lease cleanup retains
    its independent bounded revoke allowance and reports failure. A trusted host
    callback cannot be forcibly preempted and must honor its own time bound.
    Returns queue receipt count, never host approvals or executed DAW actions.
    """
    if (not isinstance(launch, launch_api.ManagedBridgeLaunch) or not callable(on_listening) or
            not callable(host_consent) or not isinstance(host_approval_ledger, NativeCommandApprovalLedger) or
            not isinstance(expected_peer_id, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", expected_peer_id) or
            not isinstance(approved_client_sha256, str) or not re.fullmatch(r"[0-9a-f]{64}", approved_client_sha256) or
            type(max_commands) is not int or not 1 <= max_commands <= 32 or
            any(type(value) not in (int, float) or not .05 <= value <= limit or not math.isfinite(value)
                for value, limit in ((session_timeout, 60), (command_timeout, 5))) or
            (cancel is not None and not isinstance(cancel, threading.Event))):
        raise ValueError("invalid managed TLS runtime configuration")
    cancel = cancel if cancel is not None else threading.Event()
    with launch_api.hold_launch_parent(launch) as parent:
        if cancel.is_set(): return 0
        with NativeTLSLoopbackReceiver(context, accept_timeout=accept_timeout,
                                       handshake_timeout=handshake_timeout) as receiver:
            stopped = threading.Event()
            interrupted = threading.Event()
            def watch_parent():
                while not stopped.wait(.025):
                    if cancel.is_set() or not launch_api.launch_parent_is_alive(parent):
                        interrupted.set();receiver.request_cancel();return
            watcher = threading.Thread(target=watch_parent, name="idw-native-parent-watch")
            watcher.start()
            try:
                if cancel.is_set() or not launch_api.launch_parent_is_alive(parent):
                    return 0
                on_listening(receiver.port)
                if interrupted.is_set() or cancel.is_set(): return 0
                def provide(identity, *, cancel, deadline):
                    return receive_windows_native_lease(launch.pipe_name, parent, identity,
                        send_offer=True, cancel=cancel, deadline=deadline)
                return receiver.serve_once(expected_peer_id=expected_peer_id,
                    approved_client_sha256=approved_client_sha256, epoch=launch.epoch,
                    host_consent=host_consent, provide_native_lease=provide,
                    host_approval_ledger=host_approval_ledger, session_timeout=session_timeout,
                    command_timeout=command_timeout, max_commands=max_commands)
            finally:
                receiver.close();stopped.set();watcher.join()
