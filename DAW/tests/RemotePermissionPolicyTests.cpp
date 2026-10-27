#include "../RemotePermissionPolicy.h"
#include <cassert>
using namespace idw::daw::remote;
int main() {
    Grant g{};
    Request r{};
    r.sessionEpoch=4; r.trackCount=6; r.trackIndex=2; r.trackIsAudio=true;
    assert(!permitted(g,r));
    g.connected=true; g.authenticated=true; g.hostApproved=true; g.sessionEpoch=4;
    assert(permitted(g,r)); // viewing allowed to admitted guest
    r.action=Action::arm_track;
    assert(!permitted(g,r)); // no control grant
    g.controlGranted=true;
    assert(permitted(g,r));
    r.trackIndex=6; assert(!permitted(g,r));
    r.trackIndex=2; r.trackIsAudio=false; assert(!permitted(g,r));
    r.trackIsAudio=true; r.transportBusy=true; assert(!permitted(g,r));
    r.transportBusy=false; r.action=Action::start_recording; assert(!permitted(g,r));
    g.recordingGranted=true; assert(permitted(g,r));
    r.recordingActive=true; assert(!permitted(g,r));
    r.action=Action::stop_recording; assert(permitted(g,r));
    r.action=Action::delete_track; assert(!permitted(g,r));
    r.action=Action::export_audio; assert(!permitted(g,r));
    r.action=Action::view; r.sessionEpoch=3; assert(!permitted(g,r));
    r.sessionEpoch=4; g.connected=false; assert(!permitted(g,r));
    g.connected=true; g.revokedEpoch=4; assert(!permitted(g,r));
    g.sessionEpoch=5; r.sessionEpoch=5; assert(permitted(g,r));
    r.actor=Role::host; assert(!permitted(g,r)); // this gate never authorizes host commands
}
