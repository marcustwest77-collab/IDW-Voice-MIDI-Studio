// Test-only process bridge; never deploy as a network listener or DAW service.
#include "../RemoteTransportSessionLifecycle.h"
#include "../RemoteValidatedCommandReviewReceiver.h"
#include <iostream>
#include <string>
using namespace idw::daw::remote;
int main() {
 SessionReviewCoordinator coordinator;
 RemoteReviewMailbox mailbox;
 TransportSessionLifecycle lifecycle(coordinator,mailbox);
 // Synthetic verified identity for this test process ONLY, never actual TLS attestation.
 if (!lifecycle.onVerifiedPeerConnected(45,true,true,true)) return 2;
 ValidatedCommandReviewReceiver receiver(coordinator);
 Request state;state.trackCount=4;state.trackIsAudio=true;
 std::string wire;
 while (std::getline(std::cin,wire)) {
   if (wire=="TEST_AUTH_LOSS") {
     lifecycle.onAuthenticationLost();
     std::cout<<"DISCONNECTED"<<std::endl;
     continue;
   }
   if (!lifecycle.onAuthenticatedTrackCommand(wire,45)) { std::cout<<"DENY"<<std::endl;continue; }
   bool accepted=false;
   for(const auto& item:mailbox.drain())
     accepted=receiver.receive(item.payload,state,item.trustedEpoch)||accepted;
   std::cout<<(accepted?"REVIEW":"DENY")<<std::endl;
 }
}
