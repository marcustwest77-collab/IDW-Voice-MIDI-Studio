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
   return coordinator_.beginHostApprovedSession(
       epoch, tlsIdentityVerified, hostConsented, allowTrackSelectionAndArm);
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
