"""Single-request, loopback-only TLS-to-native worker; trusted parent pipe only."""
import sys
from channel_bound_session import BoundSession
from host_approval_ledger import HostApprovalLedger
from native_local_command_bridge import NativeLocalCommandBridge
from replay_guard import ReplayGuard
from tls_loopback_server import serve_tls_once
from tls_worker_bootstrap import read_bootstrap
from verified_event_producer import VerifiedEventProducer


def run(config):
    peer, epoch = config["peer"], config["epoch"]
    session = BoundSession(peer, epoch, config["channel"])
    ledger = HostApprovalLedger()
    for request in config["approved_requests"]:
        ledger.approve(peer, epoch, request)
    bridge = NativeLocalCommandBridge(config["native_port"], session_token=config["token"])
    producer = VerifiedEventProducer(peer, epoch, bridge.deliver)

    def admit_verified_peer(certificate_peer, observed_epoch):
        # serve_tls_once calls this only after mandatory mTLS, pin verification,
        # and mapping the actual certificate to the configured peer identity.
        if certificate_peer != peer or observed_epoch != epoch:
            return False
        return (session.admit_bound(certificate_peer, config["channel"],
                                    config["host_approved"], True) and
                session.grant(control=config["track_control"], recording=False))

    class Ready:
        def set(self):
            print("READY", flush=True)

    serve_tls_once(
        config["tls_port"], config["server_cert"], config["server_key"], ready=Ready(),
        session=session, verified_channel_id=config["channel"], replay_guard=ReplayGuard(),
        client_cafile=config["client_ca"], approved_client_sha256=config["client_pin"],
        approved_peer_by_sha256={config["client_pin"]: peer},
        host_approval=admit_verified_peer, host_approval_ledger=ledger,
        accepted_command_sink=producer.accepted_command_sink)


def main():
    try:
        config = read_bootstrap(sys.stdin.buffer)
        run(config)
        return 0
    except Exception:
        # Do not echo secrets, paths, TLS errors, or the bootstrap on failure.
        return 1


if __name__ == "__main__":
    sys.exit(main())
