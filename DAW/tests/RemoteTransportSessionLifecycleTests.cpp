#include "../RemoteTransportSessionLifecycle.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main(){
 SessionReviewCoordinator c; RemoteReviewMailbox mailbox;
 TransportSessionLifecycle lifecycle(c, mailbox);
 // A TLS-authenticated guest without explicit host track consent is view-only.
 {
   SessionReviewCoordinator readonlyCoordinator;
   RemoteReviewMailbox readonlyMailbox;
   TransportSessionLifecycle readonly(readonlyCoordinator,readonlyMailbox);
   assert(readonly.onVerifiedPeerConnected(401,true,true,false));
   assert(readonlyCoordinator.grant().connected);
   assert(!readonlyCoordinator.grant().controlGranted);
   assert(!readonly.onAuthenticatedTrackCommand("1|401|1|forbidden|arm|0",401));
   assert(readonlyMailbox.drain().empty());
   readonly.onPeerDisconnected();
   // Reconnection with a newer epoch does not silently elevate access.
   assert(readonly.onVerifiedPeerConnected(402,true,true,false));
   assert(readonlyCoordinator.grant().sessionEpoch==402);
   assert(!readonlyCoordinator.grant().controlGranted);
   assert(!readonly.onAuthenticatedTrackCommand("1|402|1|still_denied|select|0",402));
   assert(!readonly.onAuthenticatedTrackCommand("1|401|2|stale|arm|0",401));
   assert(readonlyMailbox.drain().empty());
 }
 assert(!lifecycle.onAuthenticatedTrackCommand("1|501|1|early|arm|0",501));
 assert(!lifecycle.onVerifiedPeerConnected(501,false,true,true));
 assert(!lifecycle.onVerifiedPeerConnected(501,true,false,true));
 assert(mailbox.enqueue("1|501|1|pre_admission|arm|0",501));
 assert(lifecycle.onVerifiedPeerConnected(501,true,true,true));
 assert(mailbox.drain().empty());
 assert(c.grant().connected && c.grant().controlGranted);
 assert(!lifecycle.onAuthenticatedTrackCommand("1|501|1|wrong|arm|0",502));
 assert(lifecycle.onAuthenticatedTrackCommand("1|501|1|allowed|arm|0",501));
 assert(mailbox.drain().size()==1);
 // An unsolicited second peer must not replace the session or erase its inbox.
 assert(mailbox.enqueue("1|501|2|kept501|arm|0",501));
 assert(!lifecycle.onVerifiedPeerConnected(502,true,true,true));
 auto preserved=mailbox.drain();
 assert(preserved.size()==1 && preserved.front().trustedEpoch==501);
 assert(c.grant().sessionEpoch==501 && c.grant().controlGranted);
 CommandMessage cmd;cmd.sessionEpoch=501;cmd.sequence=1;cmd.requestId="select501";
 cmd.action=Action::select_track;cmd.trackIndex=0;
 Request state;state.trackCount=2;
 assert(c.stageInspected(cmd,state));
 assert(c.pending().size()==1);
 assert(mailbox.enqueue("1|501|3|queued501|arm|0",501));
 // Commands queued immediately before host revocation must be purged.
 assert(lifecycle.onAuthenticatedTrackCommand("1|501|5|before_revoke|arm|0",501));
 lifecycle.onHostRevokedControl();
 assert(!lifecycle.onAuthenticatedTrackCommand("1|501|4|revoked|arm|0",501));
 assert(mailbox.drain().empty());
 assert(c.pending().empty() && !c.grant().controlGranted);
 assert(c.grant().connected);
 assert(mailbox.enqueue("1|501|3|queued502|arm|0",501));
 lifecycle.onAuthenticationLost();
 assert(mailbox.drain().empty());
 assert(c.pending().empty() && !c.grant().connected);
 assert(!lifecycle.onVerifiedPeerConnected(501,true,true,true));
 assert(mailbox.enqueue("1|501|4|stale_old|arm|0",501));
 assert(lifecycle.onVerifiedPeerConnected(502,true,true,true));
 assert(mailbox.drain().empty());
 assert(mailbox.enqueue("1|502|1|queued503|arm|0",502));
 lifecycle.onPeerDisconnected();
 assert(mailbox.drain().empty());
 assert(!c.grant().connected);
}
