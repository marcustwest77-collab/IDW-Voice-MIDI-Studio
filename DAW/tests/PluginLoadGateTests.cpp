#include <iostream>
#include <stdexcept>
#include "PluginLoadGate.h"
using namespace idw::daw;
namespace {
void require(bool condition,const char* message){
    if(!condition)throw std::runtime_error(message);
}
}
int main(){
    // A load that completes in time is accepted exactly once.
    {
        PluginLoadGate gate;
        const int token=gate.begin();
        require(gate.isWaiting(),"load not pending after begin");
        const auto first=gate.complete(token);
        require(first.accepted && !first.discard,"in-time load rejected");
        require(!gate.isWaiting(),"load still pending after completion");
        const auto again=gate.complete(token);
        require(!again.accepted && again.discard,"completed load accepted twice");
        require(gate.expire(token,false)==PluginLoadGate::Timeout::ignored,
                "deadline fired after completion");
    }
    // The deadline resolves a silent load as failed, and a later callback is
    // torn down instead of starting a render.
    {
        PluginLoadGate gate;
        const int token=gate.begin();
        require(gate.expire(token,false)==PluginLoadGate::Timeout::failed,
                "deadline did not report a failure");
        require(!gate.isWaiting(),"load still pending after deadline");
        const auto late=gate.complete(token);
        require(!late.accepted && late.discard,"late load accepted");
        require(gate.expire(token,false)==PluginLoadGate::Timeout::ignored,
                "deadline resolved the same load twice");
    }
    // The deadline attributes a cancel request to the pending export.
    {
        PluginLoadGate gate;
        const int token=gate.begin();
        require(gate.expire(token,true)==PluginLoadGate::Timeout::canceled,
                "canceled wait reported as failure");
        const auto late=gate.complete(token);
        require(!late.accepted && late.discard,"canceled load accepted late");
    }
    // Cancelling during the load resolves immediately and abandons the token.
    {
        PluginLoadGate gate;
        const int token=gate.begin();
        require(gate.cancel(),"cancel did not resolve a pending load");
        require(!gate.isWaiting(),"load still pending after cancel");
        require(!gate.cancel(),"cancel resolved a load twice");
        require(gate.expire(token,false)==PluginLoadGate::Timeout::ignored,
                "deadline fired after cancel");
        const auto late=gate.complete(token);
        require(!late.accepted && late.discard,"canceled load completed");
    }
    // A superseded export's callback never reaches the new export.
    {
        PluginLoadGate gate;
        const int first=gate.begin();
        const int second=gate.begin();
        const auto stale=gate.complete(first);
        require(!stale.accepted && stale.discard,"stale load accepted for a new export");
        const auto current=gate.complete(second);
        require(current.accepted,"current load rejected");
    }
    // Cancelling with nothing pending is a no-op, so an idle DAW reports nothing.
    {
        PluginLoadGate gate;
        require(!gate.cancel(),"cancel resolved an idle export");
        require(gate.complete(1).discard,"idle gate accepted a callback");
        require(gate.expire(1,false)==PluginLoadGate::Timeout::ignored,
                "idle gate reported a deadline");
    }
    std::cout<<"PASS: plugin load deadline, cancel and stale callbacks resolve once\n";
}
