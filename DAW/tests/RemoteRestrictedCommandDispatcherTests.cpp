#include "../RemoteRestrictedCommandDispatcher.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
struct FakeAdapter : RemoteDawCommandAdapter {
 int calls=0; bool succeed=true;
 bool apply(const CommandMessage&) override { ++calls;return succeed; }
};
int main() {
 HostApprovalQueue queue;HostReviewController review(queue);
 HostApprovalDispatchGate gate(review);RestrictedCommandDispatcher dispatch(gate);
 Grant grant;grant.connected=true;grant.authenticated=true;grant.hostApproved=true;
 grant.controlGranted=true;grant.sessionEpoch=50;
 Request live;live.trackCount=5;live.trackIsAudio=true;
 CommandMessage command;command.sessionEpoch=50;command.sequence=1;
 command.requestId="r50";command.action=Action::arm_track;command.trackIndex=3;
 FakeAdapter adapter;
 assert(queue.stage(command,grant));
 assert(dispatch.dispatchAfterHostClick("r50",grant,live,adapter));
 assert(adapter.calls==1);
 assert(!dispatch.dispatchAfterHostClick("r50",grant,live,adapter));
 assert(adapter.calls==1);
 command.requestId="r51";command.sequence=2;assert(queue.stage(command,grant));
 live.transportBusy=true;
 assert(!dispatch.dispatchAfterHostClick("r51",grant,live,adapter));
 assert(adapter.calls==1);live.transportBusy=false;
 command.requestId="r52";command.sequence=3;assert(queue.stage(command,grant));
 grant.connected=false;
 assert(!dispatch.dispatchAfterHostClick("r52",grant,live,adapter));
 assert(adapter.calls==1);
}
