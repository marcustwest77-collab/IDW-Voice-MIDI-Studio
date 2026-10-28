#pragma once
// Data-only ingress for a future authenticated transport adapter. A network thread
// may enqueue facts ONLY after independent TLS identity verification; the JUCE
// message thread drains them. This queue does not authenticate any peer.
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <utility>
namespace idw::daw::remote {
struct TrustedTransportEvent {
 enum class Kind { connected, command, disconnected, authenticationLost };
 Kind kind=Kind::disconnected;
 std::uint64_t epoch=0;
 bool tlsVerified=false;
 bool hostConsented=false;
 bool trackControlGranted=false;
 std::string wire;
};
class TrustedTransportEventQueue {
public:
 static constexpr std::size_t capacity=32;
 bool push(TrustedTransportEvent event) {
  if (event.epoch==0 || event.wire.size()>4096) return false;
  if (event.kind==TrustedTransportEvent::Kind::command && event.wire.empty()) return false;
  if (event.kind!=TrustedTransportEvent::Kind::command && !event.wire.empty()) return false;
  if (event.kind==TrustedTransportEvent::Kind::connected &&
      (!event.tlsVerified || !event.hostConsented)) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (event.kind==TrustedTransportEvent::Kind::command &&
      event.epoch<=lastTerminatedEpoch_) return false;
  const bool terminal=event.kind==TrustedTransportEvent::Kind::disconnected ||
                      event.kind==TrustedTransportEvent::Kind::authenticationLost;
  if (terminal) {
   if (event.epoch>lastTerminatedEpoch_) lastTerminatedEpoch_=event.epoch;
   // Drop queued commands from the terminated epoch before delivering the
   // stop event. A flooded queue must not block authentication loss.
   for (auto it=queue_.begin();it!=queue_.end();) {
    if (it->epoch==event.epoch &&
        it->kind==TrustedTransportEvent::Kind::command) it=queue_.erase(it);
    else ++it;
   }
   if (queue_.size()>=capacity) queue_.pop_front();
  } else if (queue_.size()>=capacity) return false;
  queue_.push_back(std::move(event));
  return true;
 }
 std::deque<TrustedTransportEvent> drain() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::deque<TrustedTransportEvent> out;
  out.swap(queue_);
  return out;
 }
 void clear() { std::lock_guard<std::mutex> lock(mutex_);queue_.clear(); }
private:
 std::mutex mutex_;
 std::deque<TrustedTransportEvent> queue_;
 std::uint64_t lastTerminatedEpoch_=0;
};
}
