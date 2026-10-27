#pragma once
// Host-side C++ review controller. This does not authenticate or dispatch.
#include "RemoteControllerViewState.h"
#include <string>
#include <vector>

namespace idw::daw::remote {
struct PendingReview {
    std::string requestId;
    Action action = Action::view;
    std::size_t trackIndex = 0;
    std::uint64_t sessionEpoch = 0;
};
class HostReviewController {
public:
    explicit HostReviewController(HostApprovalQueue& queue) : queue_(queue) {}
    std::vector<PendingReview> pending(const Grant& grant) const {
        if (!grant.connected || !grant.authenticated || !grant.hostApproved ||
            grant.sessionEpoch == 0 || grant.sessionEpoch <= grant.revokedEpoch)
            return {};
        std::vector<PendingReview> result;
        for (const auto& message : queue_.pending()) {
            if (message.sessionEpoch == grant.sessionEpoch)
                result.push_back({message.requestId, message.action, message.trackIndex,
                                  message.sessionEpoch});
        }
        return result;
    }
    bool deny(const std::string& requestId) { return queue_.deny(requestId); }
    bool approve(const std::string& requestId, const Grant& grant,
                 const Request& liveState, CommandMessage& output) {
        // Host interaction must trigger this call, never a peer's network message.
        return queue_.approve(requestId, grant, liveState, output);
    }
    void disconnect() noexcept { queue_.clear(); }
private:
    HostApprovalQueue& queue_;
};
}
