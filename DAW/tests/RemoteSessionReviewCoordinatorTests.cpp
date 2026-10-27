#include "../RemoteSessionReviewCoordinator.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
 SessionReviewCoordinator coordinator;
 assert(coordinator.session().open(100));
 assert(coordinator.session().admitGuest(true,true));
 assert(coordinator.session().setPrivileges(true,false));
 Request live;live.trackCount=4;live.trackIsAudio=true;
 CommandMessage cmd;cmd.sessionEpoch=100;cmd.sequence=1;cmd.requestId="track100";
 cmd.action=Action::arm_track;cmd.trackIndex=1;
 assert(coordinator.stageInspected(cmd,live));
 assert(coordinator.pending().size()==1);
 assert(!coordinator.stageInspected(cmd,live)); // duplicate/replayed
 cmd.sequence=2;cmd.requestId="blocked";live.transportBusy=true;
 assert(!coordinator.stageInspected(cmd,live)); // live policy denies
 live.transportBusy=false;cmd.requestId="track102";
 assert(coordinator.stageInspected(cmd,live)); // rejected sequence was not committed
 cmd.sequence=3;cmd.requestId="delete";cmd.action=Action::delete_track;
 assert(!coordinator.stageInspected(cmd,live));
 coordinator.disconnect();
 assert(coordinator.pending().empty());
 assert(!coordinator.stageInspected(cmd,live));
 assert(!coordinator.session().open(100));
 assert(coordinator.session().open(101));
 assert(coordinator.session().admitGuest(true,true));
 assert(coordinator.session().setPrivileges(true,false));
 cmd.sessionEpoch=101;cmd.sequence=1;cmd.requestId="track101";
 cmd.action=Action::select_track;
 assert(coordinator.stageInspected(cmd,live));
 assert(coordinator.pending().size()==1);
}
