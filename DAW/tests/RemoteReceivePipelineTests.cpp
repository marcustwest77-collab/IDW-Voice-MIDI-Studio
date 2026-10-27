#include "../RemoteReceivePipeline.h"
#ifdef NDEBUG
#undef NDEBUG // This executable uses assert as its test runner, even in Release.
#endif
#include <cassert>
using namespace idw::daw::remote;
static bool parse(const std::string& s, CommandMessage& msg) {
    if(s!="arm2") return false;
    msg.protocolVersion=1; msg.sessionEpoch=7; msg.sequence=1;
    msg.requestId="one"; msg.action=Action::arm_track; msg.trackIndex=1;
    return true;
}
int main() {
    SessionController session; assert(session.open(7));
    assert(session.admitGuest(true,true)); assert(session.setPrivileges(true,false));
    ReceivePipeline rx(session);
    Request state; state.trackCount=3; state.trackIsAudio=true;
    CommandMessage approved;
    std::vector<std::uint8_t> frame;
    assert(encodeFrame("arm2",frame));
    assert(rx.consume(frame.data(),2,state,parse,approved)==ReceiveResult::needMore);
    assert(rx.consume(frame.data()+2,frame.size()-2,state,parse,approved)==ReceiveResult::authorized);
    assert(approved.trackIndex==1);
    assert(rx.consume(frame.data(),frame.size(),state,parse,approved)==ReceiveResult::rejected); // replay
    session.disconnect();
    assert(rx.consume(frame.data(),frame.size(),state,parse,approved)==ReceiveResult::rejected);
    rx.reset();
    assert(encodeFrame("bad",frame));
    assert(rx.consume(frame.data(),frame.size(),state,parse,approved)==ReceiveResult::failed);
    assert(rx.failed());
    rx.reset();
    unsigned char malformed[]={0xff,0xff,0xff,0xff};
    assert(rx.consume(malformed,4,state,parse,approved)==ReceiveResult::failed);
}
