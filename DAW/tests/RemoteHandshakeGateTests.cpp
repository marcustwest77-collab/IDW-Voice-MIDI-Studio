#include "../RemoteHandshakeGate.h"
#ifdef NDEBUG
#undef NDEBUG // This executable uses assert as its test runner, even in Release.
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
    SessionController session;
    assert(session.open(12));
    PeerHandshake h; h.sessionEpoch=12; h.peerId="artist-2";
    assert(admitVerifiedPeer(session,h)==HandshakeResult::denied);
    h.secureTransport=true;
    assert(admitVerifiedPeer(session,h)==HandshakeResult::denied);
    h.verifiedPeerIdentity=true;
    assert(admitVerifiedPeer(session,h)==HandshakeResult::denied);
    h.hostConfirmedPeer=true;
    assert(admitVerifiedPeer(session,h)==HandshakeResult::admitted);
    assert(session.privileges().connected);
    session.disconnect();
    assert(admitVerifiedPeer(session,h)==HandshakeResult::denied);
    assert(session.open(13));
    h.sessionEpoch=12;
    assert(admitVerifiedPeer(session,h)==HandshakeResult::denied);
    h.sessionEpoch=13; h.protocolVersion=99;
    assert(admitVerifiedPeer(session,h)==HandshakeResult::invalid);
    h.protocolVersion=1; h.peerId="../path";
    assert(admitVerifiedPeer(session,h)==HandshakeResult::invalid);
}
