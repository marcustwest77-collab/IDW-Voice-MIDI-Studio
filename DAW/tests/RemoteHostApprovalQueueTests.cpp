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
    grant.controlGranted=false;
    assert(!queue.stage(cmd,grant));
    grant.controlGranted=true;
    assert(queue.stage(cmd,grant));
    assert(!queue.stage(cmd,grant)); // duplicate
    assert(queue.size()==1);
    const auto snapshot=queue.pending();
    assert(snapshot.size()==1 && snapshot.front().requestId=="r1");
    assert(queue.deny("r1"));
    assert(snapshot.size()==1 && snapshot.front().requestId=="r1");
    assert(queue.stage(cmd,grant));
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
    assert(queue.deny("id0"));
    assert(queue.size()==15);
    assert(!queue.deny("id0"));
    assert(!queue.approve("id0",grant,live,approved));
    assert(queue.deny("id1"));
    queue.clear(); assert(queue.size()==0);
    assert(!queue.deny("id2"));
    grant.revokedEpoch=11;assert(!queue.stage(cmd,grant));
    grant.revokedEpoch=0;
    cmd.protocolVersion=2; assert(!queue.stage(cmd,grant));
    cmd.protocolVersion=1; cmd.sequence=0; assert(!queue.stage(cmd,grant));
    cmd.sequence=4; cmd.requestId="../bad"; assert(!queue.stage(cmd,grant));
    cmd.requestId="safe"; cmd.action=Action::delete_track; assert(!queue.stage(cmd,grant));
    cmd.action=Action::export_audio; assert(!queue.stage(cmd,grant));
    cmd.action=Action::start_recording; assert(!queue.stage(cmd,grant));
    cmd.action=Action::set_gain; assert(!queue.stage(cmd,grant));
    cmd.action=Action::select_track; assert(queue.stage(cmd,grant));
    assert(queue.deny("safe"));
    assert(queue.size()==0);
}
