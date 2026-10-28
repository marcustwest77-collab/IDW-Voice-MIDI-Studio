#pragma once
// Trusted transport lifecycle adapter. A boolean is NOT cryptographic verification:
// the calling implementation must verify the peer in its TLS layer first.
#include "RemoteSessionReviewCoordinator.h"
#include "RemoteReviewMailbox.h"
namespace idw::daw::remote {
class TransportSessionLifecycle {
public:
 explicit TransportSessionLifecycle(SessionReviewCoordinator& coordinator,
                                    RemoteReviewMailbox& mailbox)
     : coordinator_(coordinator), mailbox_(mailbox) {}
 bool onVerifiedPeerConnected(std::uint64_t epoch, bool tlsIdentityVerified,
                              bool hostConsented, bool allowTrackSelectionAndArm) {
   if (!tlsIdentityVerified || !hostConsented) return false;
   if (!coordinator_.beginHostApprovedSession(
       epoch, tlsIdentityVerified, hostConsented, allowTrackSelectionAndArm)) return false;
   // A newly admitted peer never inherits commands queued for a previous session.
   mailbox_.clear();
   return true;
 }
 // Called ONLY by a trusted transport adapter after cryptographic peer verification.
 // A wire command cannot authorize itself; state is owned by the local coordinator.
 bool onAuthenticatedTrackCommand(std::string payload, std::uint64_t trustedEpoch) {
   const auto& grant=coordinator_.grant();
   if (!grant.connected || !grant.authenticated || !grant.hostApproved ||
       !grant.controlGranted || trustedEpoch==0 || trustedEpoch!=grant.sessionEpoch ||
       trustedEpoch<=grant.revokedEpoch) return false;
   return mailbox_.enqueue(std::move(payload),trustedEpoch);
 }
 bool onHostRevokedControl() noexcept {
   mailbox_.clear();
   return coordinator_.revokeControl();
 }
 void onPeerDisconnected() noexcept { mailbox_.clear(); coordinator_.disconnect(); }
 void onAuthenticationLost() noexcept { mailbox_.clear(); coordinator_.disconnect(); }
private:
 SessionReviewCoordinator& coordinator_;
 RemoteReviewMailbox& mailbox_;
};
}
