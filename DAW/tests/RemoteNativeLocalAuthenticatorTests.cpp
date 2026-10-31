#include "../RemoteNativeLocalAuthenticator.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
    TrustedTransportEventQueue queue;
    NativeLocalAuthenticator auth(std::string(64,'a'),"peer",45,queue);
    auto packet=[](const std::string& wire) { return "IDWIPC1\n"+std::string(64,'a')+"\npeer\n"+wire; };
    assert(auth.configured());
    assert(!auth.accept(packet("1|45|1|before-connect|arm|0")));
    assert(queue.drain().empty());
    assert(!auth.accept(packet("CONNECT|46")));
    assert(!auth.accept(packet("CONNECT|045")));
    assert(auth.accept(packet("CONNECT|45")));
    assert(auth.accept(packet("CONNECT|45"))); // Idempotent worker restart.
    auto admission=queue.drain();
    assert(admission.size()==1);
    assert(admission.front().kind==TrustedTransportEvent::Kind::connected);
    assert(admission.front().tlsVerified && admission.front().hostConsented);
    assert(admission.front().trackControlGranted);
    assert(auth.accept(packet("1|45|1|r1|arm|0")));
    assert(!auth.accept(packet("1|45|1|replay|arm|0")));
    for (const auto& wire : {"1|45|02|r2|arm|0", "1|45|2|r2|arm|00",
          "1|46|2|r2|arm|0", "1|45|2|r2|record|0", "1|45|2|r2|arm|128",
          "1|45|18446744073709551616|r2|arm|0"}) assert(!auth.accept(packet(wire)));
    auto wrong=packet("1|45|2|r2|arm|0");
    wrong[8]='b';
    assert(!auth.accept(wrong));
    assert(!auth.accept("IDWIPC1\n"+std::string(64,'a')+"\nother\n1|45|2|r2|arm|0"));
    assert(!auth.accept(std::string(4097,'a')));
    assert(auth.accept(packet("1|45|2|r2|select|127")));
    queue.drain();
    for (std::size_t i=0;i<TrustedTransportEventQueue::capacity;++i)
        assert(queue.push({TrustedTransportEvent::Kind::command,45,false,false,false,"pending"}));
    assert(!auth.accept(packet("1|45|3|r3|arm|0")));
    queue.drain();
    assert(auth.accept(packet("1|45|3|r3|arm|0"))); // Retry a request not queued.
    assert(queue.push({TrustedTransportEvent::Kind::authenticationLost,45,false,false,false,{}}));
    assert(!auth.accept(packet("CONNECT|45")));
    assert(!auth.accept(packet("1|45|4|r4|arm|0")));
    assert(!NativeLocalAuthenticator("short","peer",45,queue).configured());

    TrustedTransportEventQueue full;
    NativeLocalAuthenticator retry(std::string(64,'a'),"peer",45,full);
    for (std::size_t i=0;i<TrustedTransportEventQueue::capacity;++i)
        assert(full.push({TrustedTransportEvent::Kind::command,45,false,false,false,"pending"}));
    assert(!retry.accept(packet("CONNECT|45")));
    assert(!full.isAdmitted(45));
    assert(!retry.accept(packet("1|45|1|early|arm|0")));
    full.drain();
    assert(retry.accept(packet("CONNECT|45")));
    assert(full.push({TrustedTransportEvent::Kind::connected,46,true,true,true,{}}));
    assert(!retry.accept(packet("CONNECT|45")));
    assert(!retry.accept(packet("1|45|2|superseded|arm|0")));
}
