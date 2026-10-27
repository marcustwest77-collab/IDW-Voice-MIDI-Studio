#pragma once
// Host-side authorization boundary, deliberately without recording execution.
// Caller must supply freshly observed DAW state for each host click.
#include "RemoteHostReviewController.h"
namespace idw::daw::remote {
class HostApprovalDispatchGate {
public:
    explicit HostApprovalDispatchGate(HostReviewController& review) : review_(review) {}
    // Consumes one pending approval. No command is executed here.
    bool approveForDispatch(const std::string& id, const Grant& verifiedGrant,
                            const Request& currentDawState, CommandMessage& out) {
        if (!verifiedGrant.connected || !verifiedGrant.authenticated ||
            !verifiedGrant.hostApproved || !verifiedGrant.controlGranted ||
            verifiedGrant.sessionEpoch == 0 ||
            verifiedGrant.sessionEpoch <= verifiedGrant.revokedEpoch) return false;
        return review_.approve(id, verifiedGrant, currentDawState, out);
    }
private:
    HostReviewController& review_;
};
}
