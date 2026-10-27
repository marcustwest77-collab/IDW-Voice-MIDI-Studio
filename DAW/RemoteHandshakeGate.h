#pragma once
// Transport-independent admission gate. Does NOT authenticate peers or establish TLS.
// The actual secure transport MUST assert verifiedPeerIdentity only after certificate/
// credential checks; an invitation code or display name is never proof of identity.
#include "RemoteSessionController.h"
#include <cstdint>
#include <string>
namespace idw::daw::remote {
enum class HandshakeResult { admitted, denied, invalid };
struct PeerHandshake {
    std::uint32_t protocolVersion = 1;
    std::uint64_t sessionEpoch = 0;
    std::string peerId;
    bool secureTransport = false;
    bool verifiedPeerIdentity = false;
    bool hostConfirmedPeer = false;
};
inline bool isSafePeerId(const std::string& id) noexcept {
    if (id.empty() || id.size() > 64) return false;
    for (unsigned char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}
inline HandshakeResult admitVerifiedPeer(SessionController& session,
                                         const PeerHandshake& hello) noexcept {
    if (hello.protocolVersion != 1 || hello.sessionEpoch == 0 ||
        !isSafePeerId(hello.peerId)) return HandshakeResult::invalid;
    if (hello.sessionEpoch != session.privileges().sessionEpoch ||
        !hello.secureTransport || !hello.verifiedPeerIdentity ||
        !hello.hostConfirmedPeer) return HandshakeResult::denied;
    return session.admitGuest(true, true) ? HandshakeResult::admitted
                                         : HandshakeResult::denied;
}
} // namespace idw::daw::remote
