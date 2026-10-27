#include "../RemoteHostApprovalQueue.h"
#ifdef NDEBUG
#undef NDEBUG // This executable uses assert as its test runner, even in Release.
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
    HostApprovalQueue queue;
    Grant grant; grant.connected=true; grant.authenticated=true; grant.hostApproved=true;
    grant.controlGranted=true; grant.sessionEpoch=11;
    Request live; live.trackCount=4; live.trackIsAudio=true;
    CommandMessage cmd; cmd.sessionEpoch=11;cmd.sequence=1;cmd.requestId="r1";
    cmd.action=Action::arm_track;cmd.trackIndex=2;
    CommandMessage approved;
    assert(queue.stage(cmd,grant));
    assert(!queue.stage(cmd,grant)); // duplicate
    assert(queue.size()==1);
    assert(!queue.approve("other",grant,live,approved));
    assert(queue.approve("r1",grant,live,approved));
    assert(approved.requestId=="r1" && queue.size()==0);
    cmd.requestId="r2"; assert(queue.stage(cmd,grant));
    grant.connected=false;
    assert(!queue.approve("r2",grant,live,approved)); // revoke after staging
    assert(queue.size()==0);
    grant.connected=true;
    cmd.requestId="r3"; assert(queue.stage(cmd,grant));
    live.trackIsAudio=false;
    assert(!queue.approve("r3",grant,live,approved)); // changed live DAW state
    live.trackIsAudio=true;
    for(int i=0;i<16;i++){cmd.requestId="id"+std::to_string(i); assert(queue.stage(cmd,grant));}
    cmd.requestId="over";
    assert(!queue.stage(cmd,grant));
    queue.clear(); assert(queue.size()==0);
    grant.revokedEpoch=11;assert(!queue.stage(cmd,grant));
}
