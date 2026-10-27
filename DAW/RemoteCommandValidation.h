#pragma once
// Remote control message validation prototype. This does not open a network port.
#include "RemotePermissionPolicy.h"
#include <cstdint>
#include <string>
#include <limits>
namespace idw::daw::remote {
struct CommandMessage {
    std::uint32_t protocolVersion = 1;
    std::uint64_t sessionEpoch = 0;
    std::uint64_t sequence = 0;
    std::string requestId;
    Action action = Action::view;
    std::size_t trackIndex = 0;
};
struct ReceiveState {
    std::uint64_t latestSequence = 0;
    std::uint64_t epoch = 0;
};
enum class Decision { accepted, invalid, stale, unauthorized };
inline bool validId(const std::string& id) noexcept {
    if (id.empty() || id.size() > 64) return false;
    for (unsigned char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}
// Caller supplies trustworthy authenticated channel identity and current DAW state.
// Only commit receive sequence after all checks pass. No command executes here.
inline Decision validateMessage(const CommandMessage& message, const Grant& grant,
                                Request request, ReceiveState& received) noexcept {
    if (message.protocolVersion != 1 || !validId(message.requestId) ||
        message.sequence == 0 || message.sessionEpoch == 0)
        return Decision::invalid;
    if (message.sessionEpoch != grant.sessionEpoch ||
        (received.epoch != 0 && received.epoch != message.sessionEpoch) ||
        message.sequence <= received.latestSequence)
        return Decision::stale;
    request.actor = Role::guest;
    request.action = message.action;
    request.trackIndex = message.trackIndex;
    request.sessionEpoch = message.sessionEpoch;
    if (!permitted(grant, request)) return Decision::unauthorized;
    received.epoch = message.sessionEpoch;
    received.latestSequence = message.sequence;
    return Decision::accepted;
}
} // namespace idw::daw::remote
