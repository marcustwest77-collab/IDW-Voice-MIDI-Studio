#include "../RemoteTransportSessionLifecycle.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main(){
 SessionReviewCoordinator c; RemoteReviewMailbox mailbox;
 TransportSessionLifecycle lifecycle(c, mailbox);
 assert(!lifecycle.onVerifiedPeerConnected(501,false,true,true));
 assert(!lifecycle.onVerifiedPeerConnected(501,true,false,true));
 assert(lifecycle.onVerifiedPeerConnected(501,true,true,true));
 assert(c.grant().connected && c.grant().controlGranted);
 CommandMessage cmd;cmd.sessionEpoch=501;cmd.sequence=1;cmd.requestId="select501";
 cmd.action=Action::select_track;cmd.trackIndex=0;
 Request state;state.trackCount=2;
 assert(c.stageInspected(cmd,state));
 assert(c.pending().size()==1);
 assert(mailbox.enqueue("1|501|2|queued501|arm|0",501));
 lifecycle.onHostRevokedControl();
 assert(mailbox.drain().empty());
 assert(c.pending().empty() && !c.grant().controlGranted);
 assert(c.grant().connected);
 assert(mailbox.enqueue("1|501|3|queued502|arm|0",501));
 lifecycle.onAuthenticationLost();
 assert(mailbox.drain().empty());
 assert(c.pending().empty() && !c.grant().connected);
 assert(!lifecycle.onVerifiedPeerConnected(501,true,true,true));
 assert(lifecycle.onVerifiedPeerConnected(502,true,true,true));
 assert(mailbox.enqueue("1|502|1|queued503|arm|0",502));
 lifecycle.onPeerDisconnected();
 assert(mailbox.drain().empty());
 assert(!c.grant().connected);
}
