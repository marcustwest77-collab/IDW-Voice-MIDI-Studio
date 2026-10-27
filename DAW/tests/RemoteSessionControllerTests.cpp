#include "../RemoteSessionController.h"
#include <cassert>
using namespace idw::daw::remote;
int main() {
    SessionController controller;
    assert(!controller.open(0));
    assert(controller.open(10));
    assert(!controller.admitGuest(false, true));
    assert(!controller.admitGuest(true, false));
    assert(!controller.setPrivileges(true, true));
    Request live; live.trackCount=3; live.trackIsAudio=true;
    CommandMessage msg; msg.sessionEpoch=10; msg.sequence=1; msg.requestId="cmd-1";
    msg.action=Action::start_recording; msg.trackIndex=1;
    assert(controller.inspect(msg, live)==Decision::unauthorized);
    assert(controller.admitGuest(true, true));
    assert(controller.setPrivileges(true, false));
    assert(controller.inspect(msg, live)==Decision::unauthorized);
    assert(controller.setPrivileges(true, true));
    assert(controller.inspect(msg, live)==Decision::accepted);
    assert(controller.inspect(msg, live)==Decision::stale);
    controller.disconnect();
    msg.sequence=2;
    assert(controller.inspect(msg, live)!=Decision::accepted);
    assert(!controller.open(10)); // no reuse of a revoked session epoch
    assert(controller.open(11));
    assert(controller.admitGuest(true, true));
    assert(controller.setPrivileges(true, false));
    msg.sessionEpoch=11; msg.sequence=1; msg.action=Action::select_track;
    assert(controller.inspect(msg, live)==Decision::accepted);
    assert(controller.lastAcceptedSequence()==1);
    controller.disconnect();
    assert(controller.lastAcceptedSequence()==0);
}
