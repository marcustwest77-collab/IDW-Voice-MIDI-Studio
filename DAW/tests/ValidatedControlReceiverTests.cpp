#include "../ValidatedControlReceiver.h"
#include <cassert>
using namespace idw::daw::remote;
int main() {
    SessionController session;
    assert(session.open(101));
    assert(session.admitGuest(true,true));
    assert(session.setPrivileges(true,false));
    ValidatedControlReceiver receiver(session);
    Request live; live.trackCount=4; live.trackIsAudio=true;
    CommandMessage out;
    std::vector<std::uint8_t> data;
    assert(encodeFrame("1|101|1|request_1|arm|2",data));
    assert(receiver.consume(data.data(),3,live,out)==ReceiveResult::needMore);
    assert(receiver.consume(data.data()+3,data.size()-3,live,out)==ReceiveResult::authorized);
    assert(out.trackIndex==2 && out.action==Action::arm_track);
    const auto previous=out;
    assert(receiver.consume(data.data(),data.size(),live,out)==ReceiveResult::rejected); // replay
    assert(out.sequence==previous.sequence);
    assert(encodeFrame("1|101|2|request_2|record|2",data));
    assert(receiver.consume(data.data(),data.size(),live,out)==ReceiveResult::rejected); // recording not granted
    assert(out.sequence==previous.sequence);
    assert(encodeFrame("1|101|2|request_2|arm|99",data));
    assert(receiver.consume(data.data(),data.size(),live,out)==ReceiveResult::rejected);
    assert(encodeFrame("1|101|2|request_2|delete|2",data));
    assert(receiver.consume(data.data(),data.size(),live,out)==ReceiveResult::failed);
    assert(receiver.failed());
    receiver.resetForAuthenticatedSession();
    assert(encodeFrame("1|101|2|request_2|arm|2",data));
    assert(receiver.consume(data.data(),data.size(),live,out)==ReceiveResult::authorized);
    session.disconnect();
    assert(encodeFrame("1|101|3|request_3|arm|2",data));
    assert(receiver.consume(data.data(),data.size(),live,out)==ReceiveResult::rejected);
}
