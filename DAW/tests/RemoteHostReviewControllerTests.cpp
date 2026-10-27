#include "../RemoteHostReviewController.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
 HostApprovalQueue queue;
 HostReviewController review(queue);
 Grant grant; grant.connected=true; grant.authenticated=true; grant.hostApproved=true;
 grant.controlGranted=true; grant.sessionEpoch=30;
 Request live; live.trackCount=4; live.trackIsAudio=true;
 CommandMessage first; first.sessionEpoch=30; first.sequence=1; first.requestId="arm1";
 first.action=Action::arm_track; first.trackIndex=2;
 assert(queue.stage(first,grant));
 assert(review.pending(grant).size()==1);
 auto shown=review.pending(grant);
 assert(shown.front().requestId=="arm1" && shown.front().trackIndex==2);
 assert(review.deny("arm1"));
 CommandMessage output;
 assert(!review.approve("arm1",grant,live,output));
 first.requestId="arm2";first.sequence=2;assert(queue.stage(first,grant));
 assert(review.approve("arm2",grant,live,output));
 assert(output.requestId=="arm2");
 first.requestId="arm3";first.sequence=3;assert(queue.stage(first,grant));
 grant.hostApproved=false;
 assert(review.pending(grant).empty());
 assert(!review.approve("arm3",grant,live,output));
 grant.hostApproved=true;
 first.requestId="arm4";first.sequence=4;assert(queue.stage(first,grant));
 review.disconnect();
 assert(review.pending(grant).empty());
 assert(!review.approve("arm4",grant,live,output));
}
