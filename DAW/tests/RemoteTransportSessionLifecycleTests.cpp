#include "../RemoteTransportSessionLifecycle.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main(){
 SessionReviewCoordinator c; TransportSessionLifecycle lifecycle(c);
 assert(!lifecycle.onVerifiedPeerConnected(501,false,true,true));
 assert(!lifecycle.onVerifiedPeerConnected(501,true,false,true));
 assert(lifecycle.onVerifiedPeerConnected(501,true,true,true));
 assert(c.grant().connected && c.grant().controlGranted);
 CommandMessage cmd;cmd.sessionEpoch=501;cmd.sequence=1;cmd.requestId="select501";
 cmd.action=Action::select_track;cmd.trackIndex=0;
 Request state;state.trackCount=2;
 assert(c.stageInspected(cmd,state));
 assert(c.pending().size()==1);
 lifecycle.onHostRevokedControl();
 assert(c.pending().empty() && !c.grant().controlGranted);
 assert(c.grant().connected);
 lifecycle.onAuthenticationLost();
 assert(c.pending().empty() && !c.grant().connected);
 assert(!lifecycle.onVerifiedPeerConnected(501,true,true,true));
 assert(lifecycle.onVerifiedPeerConnected(502,true,true,true));
 lifecycle.onPeerDisconnected();
 assert(!c.grant().connected);
}
