#include "../RemoteSessionReviewCoordinator.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main(){
 SessionReviewCoordinator coordinator;
 assert(coordinator.session().open(100));
 assert(coordinator.session().admitGuest(true,true));
 assert(coordinator.session().setPrivileges(true,false));
 CommandMessage cmd;cmd.sessionEpoch=100;cmd.sequence=1;cmd.requestId="track100";
 cmd.action=Action::arm_track;cmd.trackIndex=1;
 assert(coordinator.stageVerified(cmd));
 assert(coordinator.pending().size()==1);
 coordinator.disconnect();
 assert(coordinator.pending().empty());
 assert(!coordinator.stageVerified(cmd));
 assert(!coordinator.session().open(100));
 assert(coordinator.session().open(101));
 assert(coordinator.session().admitGuest(true,true));
 assert(coordinator.session().setPrivileges(true,false));
 assert(coordinator.pending().empty());
 cmd.sessionEpoch=101;cmd.sequence=2;cmd.requestId="track101";
 assert(coordinator.stageVerified(cmd));
 assert(coordinator.pending().size()==1);
}
