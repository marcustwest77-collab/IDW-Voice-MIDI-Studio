#pragma once
// Bounded local review queue. No sockets, no recording execution, no automatic approvals.
#include "RemoteSessionController.h"
#include <deque>
namespace idw::daw::remote {
class HostApprovalQueue {
public:
    static constexpr std::size_t capacity=16;
    // Caller must supply a command previously accepted by the receive pipeline.
    // A queue alone does not establish trust; it never dispatches.
    bool stage(const CommandMessage& command, const Grant& currentGrant) {
        if (queue_.size()>=capacity || command.protocolVersion!=1 ||
            command.sequence==0 || !validId(command.requestId) ||
            command.action==Action::delete_track || command.action==Action::export_audio ||
            command.sessionEpoch==0 ||
            !currentGrant.connected || !currentGrant.authenticated ||
            !currentGrant.hostApproved || command.sessionEpoch!=currentGrant.sessionEpoch ||
            command.sessionEpoch<=currentGrant.revokedEpoch) return false;
        for (const auto& existing : queue_)
            if (existing.sessionEpoch==command.sessionEpoch &&
                existing.requestId==command.requestId) return false;
        queue_.push_back(command);
        return true;
    }
    // An explicit host click should call approve; caller must recheck trusted
    // live DAW state. Approved data still requires a separately-reviewed dispatcher.
    bool approve(const std::string& requestId, const Grant& grant,
                 const Request& liveState, CommandMessage& approved) {
        for (auto it=queue_.begin();it!=queue_.end();++it) {
            if (it->requestId!=requestId) continue;
            const auto candidate=*it;
            queue_.erase(it);  // never retry a dismissed request automatically
            Request request=liveState;
            request.actor=Role::guest;
            request.action=candidate.action;
            request.trackIndex=candidate.trackIndex;
            request.sessionEpoch=candidate.sessionEpoch;
            if (!permitted(grant,request)) return false;
            approved=candidate;
            return true;
        }
        return false;
    }
    void clear() noexcept { queue_.clear(); }
    std::size_t size() const noexcept { return queue_.size(); }
private:
    std::deque<CommandMessage> queue_;
};
}
