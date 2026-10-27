#include "../RemoteControllerViewState.h"
#ifdef NDEBUG
#undef NDEBUG // Assertions are the test runner, including in Release.
#endif
#include <cassert>
using namespace idw::daw::remote;
int main(){
    Grant g; HostApprovalQueue queue;
    auto view=controllerViewState(g,queue);
    assert(!view.connected && !view.canApproveRequests && view.pending==0);
    g.sessionEpoch=10; g.connected=true; g.authenticated=true; g.hostApproved=true;
    g.controlGranted=true; g.recordingGranted=true;
    CommandMessage m; m.sessionEpoch=10; m.sequence=1; m.requestId="r1"; m.action=Action::arm_track;
    assert(queue.stage(m,g));
    view=controllerViewState(g,queue);
    assert(view.connected && view.recordingEnabled && view.pending==1 && view.canApproveRequests);
    g.controlGranted=false;
    view=controllerViewState(g,queue);
    assert(!view.controlEnabled && !view.recordingEnabled && !view.canApproveRequests);
    g.revokedEpoch=10;
    view=controllerViewState(g,queue);
    assert(!view.connected && !view.authenticated && view.pending==0);
}
