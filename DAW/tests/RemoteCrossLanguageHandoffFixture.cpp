// Test-only process bridge; never deploy as a network listener or DAW service.
#include "../RemoteReviewMailbox.h"
#include "../RemoteValidatedCommandReviewReceiver.h"
#include <iostream>
#include <string>
using namespace idw::daw::remote;
int main() {
 SessionReviewCoordinator coordinator;
 if (!coordinator.beginHostApprovedSession(45,true,true,true)) return 2;
 RemoteReviewMailbox mailbox;
 ValidatedCommandReviewReceiver receiver(coordinator);
 Request state;state.trackCount=4;state.trackIsAudio=true;
 std::string wire;
 while (std::getline(std::cin,wire)) {
   if (!mailbox.enqueue(wire,45)) { std::cout<<"DENY\n";continue; }
   bool accepted=false;
   for(const auto& item:mailbox.drain())
     accepted=receiver.receive(item.payload,state,item.trustedEpoch)||accepted;
   std::cout<<(accepted?"REVIEW":"DENY")<<"\n";
 }
}
