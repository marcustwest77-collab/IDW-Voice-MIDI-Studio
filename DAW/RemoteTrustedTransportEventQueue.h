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
  if ((event.kind==TrustedTransportEvent::Kind::command ||
       event.kind==TrustedTransportEvent::Kind::connected) &&
      event.epoch<=lastTerminatedEpoch_) return false;
  // Once a newer authenticated admission is queued, a delayed older
  // command must never be placed after it and processed as current.
  if ((event.kind==TrustedTransportEvent::Kind::command ||
       event.kind==TrustedTransportEvent::Kind::connected) &&
      event.epoch<highestAdmittedEpoch_) return false;
  const bool terminal=event.kind==TrustedTransportEvent::Kind::disconnected ||
                      event.kind==TrustedTransportEvent::Kind::authenticationLost;
  if (terminal) {
   if (event.epoch>lastTerminatedEpoch_) lastTerminatedEpoch_=event.epoch;
   // Drop queued commands from the terminated epoch before delivering the
   // stop event. A flooded queue must not block authentication loss.
   for (auto it=queue_.begin();it!=queue_.end();) {
    if (it->epoch==event.epoch &&
        (it->kind==TrustedTransportEvent::Kind::command ||
         it->kind==TrustedTransportEvent::Kind::connected)) it=queue_.erase(it);
    else ++it;
   }
   if (queue_.size()>=capacity) {
    // Preserve older termination notifications: evict low-priority traffic
    // first, rather than silently losing a prior authentication-loss event.
    auto it=queue_.begin();
    while (it!=queue_.end() &&
           (it->kind==TrustedTransportEvent::Kind::disconnected ||
            it->kind==TrustedTransportEvent::Kind::authenticationLost)) ++it;
    if (it!=queue_.end()) queue_.erase(it);
    else queue_.pop_front(); // Queue contains only terminal events.
   }
  } else if (queue_.size()>=capacity) return false;
  if (event.kind==TrustedTransportEvent::Kind::connected) {
   highestAdmittedEpoch_=event.epoch;
   admittedTrackControl_=event.trackControlGranted;
  }
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
 // Final local-host dispatch boundary. Serialize a short, synchronous action
 // with admission/termination so a stale UI grant cannot outlive accepted facts.
 // Callback must not re-enter this queue, perform I/O, or wait for a producer.
 // UI refresh happens after this returns. Caller enforces JUCE message-thread use.
 template <typename Callback>
 bool withActiveControlEpoch(std::uint64_t epoch, Callback&& callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (epoch==0 || epoch!=highestAdmittedEpoch_ || epoch<=lastTerminatedEpoch_ ||
      !admittedTrackControl_) return false;
  return std::forward<Callback>(callback)();
 }
private:
 std::mutex mutex_;
 std::deque<TrustedTransportEvent> queue_;
 std::uint64_t lastTerminatedEpoch_=0;
 std::uint64_t highestAdmittedEpoch_=0;
 bool admittedTrackControl_=false;
};
}
