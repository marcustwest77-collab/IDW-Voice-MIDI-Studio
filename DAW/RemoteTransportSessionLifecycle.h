#pragma once
// Trusted transport lifecycle adapter. A boolean is NOT cryptographic verification:
// the calling implementation must verify the peer in its TLS layer first.
#include "RemoteSessionReviewCoordinator.h"
namespace idw::daw::remote {
class TransportSessionLifecycle {
public:
 explicit TransportSessionLifecycle(SessionReviewCoordinator& coordinator) : coordinator_(coordinator) {}
 bool onVerifiedPeerConnected(std::uint64_t epoch, bool tlsIdentityVerified,
                              bool hostConsented, bool allowTrackSelectionAndArm) {
   if (!tlsIdentityVerified || !hostConsented) return false;
   return coordinator_.beginHostApprovedSession(
       epoch, tlsIdentityVerified, hostConsented, allowTrackSelectionAndArm);
 }
 void onHostRevokedControl() noexcept { coordinator_.revokeControl(); }
 void onPeerDisconnected() noexcept { coordinator_.disconnect(); }
 void onAuthenticationLost() noexcept { coordinator_.disconnect(); }
private:
 SessionReviewCoordinator& coordinator_;
};
}
