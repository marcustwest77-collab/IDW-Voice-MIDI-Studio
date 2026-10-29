#include "../RemoteVerifiedLocalCommandQueueAdapter.h"
#include <cassert>

int main() {
    using namespace idw::daw::remote;
    TrustedTransportEventQueue queue;
    VerifiedLocalCommandQueueAdapter adapter(queue);
    assert(queue.push({TrustedTransportEvent::Kind::connected, 45, true, true, true, {}}));
    assert(adapter.submitValidatedCommand("1|45|1|review|arm|1", 45));
    assert(!adapter.submitValidatedCommand("", 45));
    assert(!adapter.submitValidatedCommand("1|45|2|review|arm|1", 0));
    assert(!adapter.submitValidatedCommand("1|46|2|wrong_epoch|arm|1",45));
    assert(!adapter.submitValidatedCommand("1|45|2|bad_record|record|1",45));
    assert(!adapter.submitValidatedCommand("1|45|2|bad_track|arm|128",45));
    assert(!adapter.submitValidatedCommand("broken",45));
    const auto items=queue.drain();
    assert(items.size()==2);
    assert(items.back().kind==TrustedTransportEvent::Kind::command);
    assert(items.back().epoch==45);
    assert(items.back().wire=="1|45|1|review|arm|1");
    assert(queue.push({TrustedTransportEvent::Kind::authenticationLost,45,false,false,false,{}}));
    assert(!adapter.submitValidatedCommand("1|45|3|late|arm|1",45));
}
