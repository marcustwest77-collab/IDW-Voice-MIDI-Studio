#pragma once
// In-memory coordinator only; transport must authenticate the peer externally.
#include "RemoteSessionController.h"
#include "RemoteHostReviewController.h"
namespace idw::daw::remote {
class SessionReviewCoordinator {
public:
    SessionController& session() noexcept { return session_; }
    const SessionController& session() const noexcept { return session_; }
    HostReviewController& reviews() noexcept { return reviews_; }
    // Call only after the transport verifies the guest identity. The trusted
    // DAW caller supplies current track/transport state, never peer assertions.
    bool stageInspected(const CommandMessage& command, const Request& liveState) {
        const auto& grant=session_.privileges();
        if (!grant.connected || !grant.authenticated || !grant.hostApproved ||
            !grant.controlGranted) return false;
        // Reject disallowed actions before committing the receive sequence.
        if (command.action != Action::select_track &&
            command.action != Action::arm_track) return false;
        // Reject queue overflow and duplicate request IDs without consuming a sequence.
        if (queue_.size() >= HostApprovalQueue::capacity) return false;
        for (const auto& pending : queue_.pending())
            if (pending.sessionEpoch == command.sessionEpoch &&
                pending.requestId == command.requestId) return false;
        if (session_.inspect(command, liveState) != Decision::accepted) return false;
        return queue_.stage(command, grant);
    }
    void disconnect() noexcept {
        session_.disconnect();
        reviews_.disconnect();
    }
    std::vector<PendingReview> pending() const {
        return reviews_.pending(session_.privileges());
    }
private:
    SessionController session_;
    HostApprovalQueue queue_;
    HostReviewController reviews_{queue_};
};
}
