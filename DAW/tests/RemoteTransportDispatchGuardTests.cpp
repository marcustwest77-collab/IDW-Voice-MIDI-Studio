#include "../RemoteRestrictedCommandDispatcher.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <stdexcept>
#include <thread>
using namespace idw::daw::remote;
static TrustedTransportEvent admission(std::uint64_t epoch, bool control=true) {
 TrustedTransportEvent e;e.kind=TrustedTransportEvent::Kind::connected;
 e.epoch=epoch;e.tlsVerified=true;e.hostConsented=true;e.trackControlGranted=control;return e;
}
static TrustedTransportEvent loss(std::uint64_t epoch) {
 TrustedTransportEvent e;e.kind=TrustedTransportEvent::Kind::authenticationLost;e.epoch=epoch;return e;
}
struct Adapter : RemoteDawCommandAdapter {
 int calls=0;bool succeed=true;
 bool apply(const CommandMessage&) override {++calls;return succeed;}
};
struct Harness {
 HostApprovalQueue pending;HostReviewController review{pending};
 HostApprovalDispatchGate gate{review};RestrictedCommandDispatcher dispatcher{gate};
 TrustedTransportEventQueue transport;Grant grant;Request live;Adapter adapter;
 Harness() {
  grant.connected=true;grant.authenticated=true;grant.hostApproved=true;
  grant.controlGranted=true;grant.sessionEpoch=50;live.trackCount=5;live.trackIsAudio=true;
 }
 void stage() {
  CommandMessage c;c.sessionEpoch=50;c.sequence=1;c.requestId="r50";
  c.action=Action::arm_track;c.trackIndex=3;assert(pending.stage(c,grant));
 }
 bool dispatch() {return dispatcher.dispatchWithActiveTransport("r50",grant,live,adapter,transport);}
};
int main() {
 {
  Harness h;h.stage();assert(!h.dispatch());assert(h.pending.size()==1);
  assert(h.transport.push(admission(50)));h.transport.drain();
  assert(h.dispatch());assert(!h.dispatch());assert(h.adapter.calls==1);
 }
 {
  Harness h;h.stage();assert(h.transport.push(admission(50)));
  assert(h.transport.push(loss(50))); // UI grant deliberately remains stale; no drain.
  assert(!h.dispatch());assert(h.adapter.calls==0);assert(h.pending.size()==1);
 }
 {
  Harness h;h.stage();assert(h.transport.push(admission(50)));
  assert(h.transport.push(admission(51)));assert(!h.dispatch());assert(h.adapter.calls==0);
 }
 {
  Harness h;h.stage();assert(h.transport.push(admission(50)));
  assert(h.transport.push(admission(50,false)));assert(!h.dispatch());
  assert(h.adapter.calls==0);
 }
 {
  Harness h;h.stage();assert(h.transport.push(admission(50)));
  for (std::size_t i=1;i<TrustedTransportEventQueue::capacity;++i) {
   TrustedTransportEvent e;e.kind=TrustedTransportEvent::Kind::command;e.epoch=50;e.wire="queued";
   assert(h.transport.push(e));
  }
  assert(!h.transport.push(admission(51,false)));assert(h.dispatch());
 }
 {
  TrustedTransportEventQueue q;assert(q.push(admission(51)));assert(q.push(loss(50)));
  int calls=0;auto action=[&]{++calls;return true;};
  assert(!q.withActiveControlEpoch(0,action));assert(!q.withActiveControlEpoch(52,action));
  assert(q.withActiveControlEpoch(51,action));assert(calls==1);
  try {q.withActiveControlEpoch(51,[]() -> bool {throw std::runtime_error("test");});}
  catch(const std::runtime_error&) {}
  assert(q.push(loss(51)));assert(!q.withActiveControlEpoch(51,action));
 }
 {
  Harness h;h.stage();assert(h.transport.push(admission(50)));h.adapter.succeed=false;
  assert(!h.dispatch());assert(!h.dispatch());assert(h.adapter.calls==1);assert(h.pending.size()==0);
 }
 {
  Harness h;h.stage();assert(h.transport.push(admission(50)));h.live.transportBusy=true;
  assert(!h.dispatch());assert(h.adapter.calls==0);assert(h.pending.size()==0);
 }
 // An in-progress mutation precedes accepted termination. Test-only waiting
 // deliberately holds the guard; production callbacks must never wait or do I/O.
 {
  TrustedTransportEventQueue q;assert(q.push(admission(50)));
  std::mutex mutex;std::condition_variable cv;bool entered=false,release=false,attempted=false;
  std::atomic<bool> finished{false},returned{false},ordered{false};
  std::thread action([&]{assert(q.withActiveControlEpoch(50,[&]{
   std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_all();
   assert(cv.wait_for(lock,std::chrono::seconds(5),[&]{return release;}));
   finished=true;return true;
  }));});
  {std::unique_lock<std::mutex> lock(mutex);
   assert(cv.wait_for(lock,std::chrono::seconds(5),[&]{return entered;}));}
  std::thread terminal([&]{
   {std::lock_guard<std::mutex> lock(mutex);attempted=true;cv.notify_all();}
   assert(q.push(loss(50)));ordered=finished.load();returned=true;
  });
  {std::unique_lock<std::mutex> lock(mutex);
   assert(cv.wait_for(lock,std::chrono::seconds(5),[&]{return attempted;}));
   assert(!returned);release=true;cv.notify_all();}
  action.join();terminal.join();assert(ordered);assert(returned);
  assert(!q.withActiveControlEpoch(50,[]{assert(false);return true;}));
 }
}
