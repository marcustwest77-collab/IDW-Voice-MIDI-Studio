#include "../RemoteValidatedCommandReviewReceiver.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
 SessionReviewCoordinator coordinator;
 ValidatedCommandReviewReceiver receiver(coordinator);
 Request live;live.trackCount=3;live.trackIsAudio=true;
 assert(!receiver.receive("1|250|1|req1|arm|1",live,250));
 assert(coordinator.beginHostApprovedSession(250,true,true,true));
 assert(!receiver.receive("1|250|1|forbidden|record|1",live,250));
 assert(coordinator.pending().empty());
 assert(!receiver.receive("1|250|1|req1|arm|1",live,249));
 assert(receiver.receive("1|250|1|req1|arm|1",live,250));
 assert(coordinator.pending().size()==1);
 assert(!receiver.receive("1|250|1|req1|arm|1",live,250));
 assert(!receiver.receive("1|250|2|wrong|arm|3",live,250));
 assert(receiver.receive("1|250|2|req2|select|0",live,250));
 coordinator.revokeControl();
 assert(coordinator.pending().empty());
 assert(!receiver.receive("1|250|3|after_revoke|arm|1",live,250));
 coordinator.disconnect();
 assert(!receiver.receive("1|250|3|after_disconnect|arm|1",live,250));
}
