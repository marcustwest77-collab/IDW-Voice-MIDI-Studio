#include "../RemoteReviewMailbox.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <string>
#include <thread>
#include <atomic>
using namespace idw::daw::remote;
int main() {
 RemoteReviewMailbox box;
 assert(!box.enqueue("",101));
 assert(!box.enqueue("1|101|1|a|arm|0",0));
 assert(!box.enqueue(std::string(4097,'x'),101));
 assert(!box.enqueue("1|101|1|a|arm|0",102));
 assert(!box.enqueue("1|101|1|a|record|0",101));
 assert(box.enqueue("1|101|1|a|arm|0",101));
 assert(!box.enqueue("1|101|2|a|select|0",101));
 assert(!box.enqueue("1|101|1|other|select|0",101));
 for(std::size_t i=1;i<RemoteReviewMailbox::capacity;++i)
   assert(box.enqueue("1|101|"+std::to_string(i+1)+"|id"+std::to_string(i)+"|arm|0",101));
 assert(!box.enqueue("1|101|2|b|select|0",101));
 auto items=box.drain();
 assert(items.size()==RemoteReviewMailbox::capacity);
 assert(items.front().trustedEpoch==101);
 assert(box.drain().empty());
 // Draining the UI queue is not a new session: old wire frames stay rejected.
 assert(!box.enqueue("1|101|16|replayed|arm|0",101));
 assert(!box.enqueue("1|101|2|replayed_low|arm|0",101));
 assert(box.enqueue("1|101|17|fresh|arm|0",101));
 assert(box.drain().size()==1);
 assert(box.enqueue("1|102|1|c|select|0",102));
 assert(!box.enqueue("1|101|18|old_epoch|arm|0",101));
 assert(box.drain().size()==1);
 assert(!box.enqueue("1|101|19|old_after_drain|arm|0",101));
 box.clear();
 assert(box.drain().empty());
 // Exercise producer/consumer access concurrently with distinct IDs.
 RemoteReviewMailbox concurrent;
 std::atomic<bool> producerDone{false};
 std::atomic<int> accepted{0};
 std::atomic<int> delivered{0};
 std::thread producer([&] {
   for (int i=1;i<=100;++i) {
     const auto wire="1|900|"+std::to_string(i)+"|req"+std::to_string(i)+"|arm|0";
     if (concurrent.enqueue(wire,900)) ++accepted;
     std::this_thread::yield();
   }
   producerDone=true;
 });
 std::thread consumer([&] {
   while (!producerDone.load()) {
     delivered+=static_cast<int>(concurrent.drain().size());
     std::this_thread::yield();
   }
   delivered+=static_cast<int>(concurrent.drain().size());
 });
 producer.join(); consumer.join();
 assert(accepted==delivered);
 assert(accepted>0);
 assert(concurrent.drain().empty());
}
