#pragma once
#include "RemoteControlCommandParser.h"
#include "RemoteVerifiedLocalCommandQueueAdapter.h"

namespace idw::daw::remote {
// Native IPC v1: IDWIPC1\n<64 hex token>\n<peer>\n<canonical command>.
// Framed with the existing four-byte big-endian length, at most 4096 bytes.
// Configuration comes from trusted local code, never from an incoming packet.
class NativeLocalAuthenticator {
public:
    NativeLocalAuthenticator(std::string token, std::string peer, std::uint64_t epoch,
                             TrustedTransportEventQueue& queue)
        : token_(std::move(token)), peer_(std::move(peer)), epoch_(epoch), adapter_(queue) {}
    bool configured() const {
        if (token_.size()!=64 || !validId(peer_) || epoch_==0) return false;
        for (char c : token_)
            if (!((c>='0' && c<='9') || (c>='a' && c<='f'))) return false;
        return true;
    }
    bool accept(const std::string& packet) {
        if (!configured() || packet.size()>4096 || packet.compare(0,8,"IDWIPC1\n")!=0)
            return false;
        if (packet.size()<74 || packet[72]!='\n') return false;
        unsigned difference=0;
        for (std::size_t i=0;i<64;++i)
            difference |= static_cast<unsigned char>(packet[8+i]) ^
                          static_cast<unsigned char>(token_[i]);
        if (difference!=0) return false;
        const auto endPeer=packet.find('\n',73);
        if (endPeer==std::string::npos || packet.substr(73,endPeer-73)!=peer_) return false;
        const auto wire=packet.substr(endPeer+1);
        CommandMessage command;
        if (!parseControlCommand(wire,command) || command.sessionEpoch!=epoch_ ||
            command.sequence<=sequence_ || command.trackIndex>=128 ||
            (command.action!=Action::select_track && command.action!=Action::arm_track))
            return false;
        const auto canonical="1|"+std::to_string(epoch_)+"|"+std::to_string(command.sequence)+
            "|"+command.requestId+"|"+(command.action==Action::arm_track ? "arm" : "select")+
            "|"+std::to_string(command.trackIndex);
        if (wire!=canonical || !adapter_.submitValidatedCommand(wire,epoch_)) return false;
        sequence_=command.sequence; // A full/revoked queue must not acknowledge delivery.
        return true;
    }
private:
    std::string token_,peer_;
    std::uint64_t epoch_=0,sequence_=0;
    VerifiedLocalCommandQueueAdapter adapter_;
};
}
