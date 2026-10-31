#include "../RemoteNativeLocalAuthenticator.h"
#include "../RemoteTransportSessionLifecycle.h"
#include "../RemoteValidatedCommandReviewReceiver.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
    TrustedTransportEventQueue queue;
    NativeLocalAuthenticator auth(std::string(64,'a'),"peer",45,queue);
    SessionReviewCoordinator coordinator;
    RemoteReviewMailbox mailbox;
    TransportSessionLifecycle lifecycle(coordinator,mailbox);
    ValidatedCommandReviewReceiver reviewer(coordinator);
    auto packet=[](const std::string& body) {
        return "IDWIPC1\n"+std::string(64,'a')+"\npeer\n"+body;
    };
    assert(!coordinator.grant().connected);
    assert(!auth.accept(packet("1|45|1|before|arm|0")));
    assert(queue.drain().empty());
    // A real worker invokes this only after its TLS certificate checks. The
    // standalone test exercises the native-to-message-thread handoff itself.
    assert(auth.accept(packet("CONNECT|45")));
    assert(auth.accept(packet("1|45|1|first|arm|0")));
    assert(!coordinator.grant().connected); // Worker never mutates UI authority.
    auto events=queue.drain();
    assert(events.size()==2);
    assert(events.front().kind==TrustedTransportEvent::Kind::connected);
    for (const auto& event : events) {
        if (event.kind==TrustedTransportEvent::Kind::connected)
            assert(lifecycle.onVerifiedPeerConnected(event.epoch,event.tlsVerified,
                                                     event.hostConsented,event.trackControlGranted));
        else
            assert(lifecycle.onAuthenticatedTrackCommand(event.wire,event.epoch));
    }
    assert(coordinator.grant().connected && !coordinator.grant().recordingGranted);
    Request live;
    live.trackCount=2;
    live.trackIsAudio=true;
    for (const auto& pending : mailbox.drain())
        assert(reviewer.receive(pending.payload,live,pending.trustedEpoch));
    assert(coordinator.pending().size()==1); // Pending host decision; not executed.
    assert(auth.accept(packet("CONNECT|45")));
    assert(queue.drain().empty()); // Worker restart cannot reset host permissions.
    assert(queue.push({TrustedTransportEvent::Kind::disconnected,45,false,false,false,{}}));
    lifecycle.onPeerDisconnected();
    assert(!auth.accept(packet("CONNECT|45")));
    assert(!auth.accept(packet("1|45|2|late|arm|0")));
    assert(coordinator.pending().empty() && !coordinator.grant().connected);
}
