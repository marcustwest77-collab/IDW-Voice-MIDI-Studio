#include "../RemoteHostApprovalDispatchGate.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
 HostApprovalQueue queue;
 HostReviewController review(queue);
 HostApprovalDispatchGate gate(review);
 Grant grant;grant.connected=true;grant.authenticated=true;grant.hostApproved=true;
 grant.controlGranted=true;grant.sessionEpoch=40;
 Request state;state.trackCount=4;state.trackIsAudio=true;
 CommandMessage cmd;cmd.sessionEpoch=40;cmd.sequence=1;cmd.requestId="req40";
 cmd.action=Action::arm_track;cmd.trackIndex=2;
 CommandMessage out;
 assert(queue.stage(cmd,grant));
 Grant invalid=grant;invalid.controlGranted=false;
 assert(!gate.approveForDispatch("req40",invalid,state,out));
 assert(queue.size()==1);
 state.transportBusy=true;
 assert(!gate.approveForDispatch("req40",grant,state,out));
 assert(queue.size()==0);
 state.transportBusy=false;cmd.requestId="req41";cmd.sequence=2;
 assert(queue.stage(cmd,grant));
 assert(gate.approveForDispatch("req41",grant,state,out));
 assert(out.requestId=="req41" && queue.size()==0);
 assert(!gate.approveForDispatch("req41",grant,state,out));
 cmd.requestId="req42";cmd.sequence=3;
 assert(queue.stage(cmd,grant));
 grant.revokedEpoch=40;
 assert(!gate.approveForDispatch("req42",grant,state,out));
}
