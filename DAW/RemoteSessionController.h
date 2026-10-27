#pragma once
// In-memory remote-session state machine, NOT a network or authentication implementation.
// Identity verification must happen in a separately reviewed transport before admitGuest.
#include "RemoteCommandValidation.h"
#include <cstdint>
namespace idw::daw::remote {
class SessionController {
public:
    // A host creates a fresh nonzero epoch supplied by a secure session coordinator.
    // Never reuse an epoch after disconnect/revocation.
    bool open(std::uint64_t newEpoch) noexcept {
        if (newEpoch == 0 || newEpoch <= highestEpoch_) return false;
        highestEpoch_ = newEpoch;
        grant_ = {};
        received_ = {};
        grant_.sessionEpoch = newEpoch;
        return true;
    }
    // Authenticated is asserted by the calling transport; never infer it from an invite code.
    bool admitGuest(bool transportAuthenticated, bool hostApproved) noexcept {
        if (grant_.sessionEpoch == 0 || !transportAuthenticated || !hostApproved)
            return false;
        grant_.connected = true;
        grant_.authenticated = true;
        grant_.hostApproved = true;
        return true;
    }
    bool setPrivileges(bool control, bool recording) noexcept {
        if (!grant_.connected || !grant_.authenticated || !grant_.hostApproved)
            return false;
        grant_.controlGranted = control;
        grant_.recordingGranted = control && recording;
        return true;
    }
    void disconnect() noexcept {
        grant_.revokedEpoch = grant_.sessionEpoch;
        grant_.connected = false;
        grant_.authenticated = false;
        grant_.hostApproved = false;
        grant_.controlGranted = false;
        grant_.recordingGranted = false;
        received_ = {};
    }
    // Only performs authorization and anti-replay checks. It does not dispatch commands.
    Decision inspect(const CommandMessage& command, const Request& liveState) noexcept {
        return validateMessage(command, grant_, liveState, received_);
    }
    const Grant& privileges() const noexcept { return grant_; }
    std::uint64_t lastAcceptedSequence() const noexcept {
        return received_.latestSequence;
    }
private:
    Grant grant_{};
    ReceiveState received_{};
    std::uint64_t highestEpoch_ = 0;
};
} // namespace idw::daw::remote
