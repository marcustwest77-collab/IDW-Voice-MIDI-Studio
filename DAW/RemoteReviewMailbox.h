#pragma once
// Thread-safe, data-only mailbox. Authenticated transport may enqueue a wire
// command; the DAW message thread must drain and revalidate live state.
// This component neither authenticates peers nor executes audio actions.
#include "RemoteControlCommandParser.h"
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
namespace idw::daw::remote {
struct PendingTransportCommand {
 std::string payload;
 std::uint64_t trustedEpoch=0;
};
class RemoteReviewMailbox {
public:
 static constexpr std::size_t capacity=16;
 bool enqueue(std::string serialized, std::uint64_t trustedEpoch) {
  if (trustedEpoch==0 || serialized.empty() || serialized.size()>4096) return false;
  // Reject malformed or cross-session commands before allocating queue space.
  CommandMessage parsed;
  if (!parseControlCommand(serialized, parsed) ||
      parsed.sessionEpoch != trustedEpoch ||
      (parsed.action != Action::select_track && parsed.action != Action::arm_track))
      return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.size()>=capacity) return false;
  // Retain the last accepted sequence even after the review UI drains this queue.
  // Otherwise a replay with a lower sequence can enter an empty mailbox.
  if (trustedEpoch<lastEpoch_ ||
      (trustedEpoch==lastEpoch_ && parsed.sequence<=lastSequence_)) return false;
  for (const auto& pending : queue_) {
   CommandMessage existing;
   if (pending.trustedEpoch==trustedEpoch &&
       parseControlCommand(pending.payload, existing) &&
       (existing.requestId==parsed.requestId ||
        parsed.sequence<=existing.sequence)) return false;
  }
  queue_.push_back({std::move(serialized),trustedEpoch});
  lastEpoch_=trustedEpoch;
  lastSequence_=parsed.sequence;
  return true;
 }
 std::deque<PendingTransportCommand> drain() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::deque<PendingTransportCommand> result;
  result.swap(queue_);
  return result;
 }
 void clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  queue_.clear();
  lastEpoch_=0;
  lastSequence_=0;
 }
private:
 std::mutex mutex_;
 std::deque<PendingTransportCommand> queue_;
 std::uint64_t lastEpoch_=0;
 std::uint64_t lastSequence_=0;
};
}
