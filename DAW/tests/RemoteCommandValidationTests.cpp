#include "../RemoteCommandValidation.h"
#ifdef NDEBUG
#undef NDEBUG // This executable uses assert as its test runner, even in Release.
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
    Grant grant; grant.connected=true; grant.authenticated=true; grant.hostApproved=true;
    grant.controlGranted=true; grant.recordingGranted=false; grant.sessionEpoch=7;
    Request request; request.trackCount=4; request.trackIsAudio=true;
    ReceiveState received;
    CommandMessage message; message.sessionEpoch=7; message.sequence=1;
    message.requestId="req-1"; message.action=Action::arm_track; message.trackIndex=1;
    assert(validateMessage(message,grant,request,received)==Decision::accepted);
    assert(received.latestSequence==1);
    assert(validateMessage(message,grant,request,received)==Decision::stale);
    message.sequence=2; message.requestId="../bad";
    assert(validateMessage(message,grant,request,received)==Decision::invalid);
    assert(received.latestSequence==1);
    message.requestId="req-2"; message.trackIndex=99;
    assert(validateMessage(message,grant,request,received)==Decision::unauthorized);
    assert(received.latestSequence==1);
    message.trackIndex=2; message.action=Action::start_recording;
    assert(validateMessage(message,grant,request,received)==Decision::unauthorized);
    message.action=Action::delete_track;
    assert(validateMessage(message,grant,request,received)==Decision::unauthorized);
    message.action=Action::arm_track; grant.connected=false;
    assert(validateMessage(message,grant,request,received)==Decision::unauthorized);
    grant.connected=true; grant.revokedEpoch=7;
    assert(validateMessage(message,grant,request,received)==Decision::unauthorized);
    grant.revokedEpoch=0; message.sessionEpoch=8;
    assert(validateMessage(message,grant,request,received)==Decision::stale);
    message.sessionEpoch=7;
    assert(validateMessage(message,grant,request,received)==Decision::accepted);
    assert(received.latestSequence==2);
}
