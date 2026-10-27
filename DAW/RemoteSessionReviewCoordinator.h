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
    bool stageVerified(const CommandMessage& command) {
        const auto& grant=session_.privileges();
        if (!grant.connected || !grant.authenticated || !grant.hostApproved ||
            !grant.controlGranted) return false;
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
