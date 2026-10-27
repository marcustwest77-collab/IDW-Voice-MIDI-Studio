#include "../RemoteReviewMailbox.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <string>
using namespace idw::daw::remote;
int main() {
 RemoteReviewMailbox box;
 assert(!box.enqueue("",101));
 assert(!box.enqueue("1|101|1|a|arm|0",0));
 assert(!box.enqueue(std::string(4097,'x'),101));
 for(std::size_t i=0;i<RemoteReviewMailbox::capacity;++i)
   assert(box.enqueue("1|101|1|a|arm|0",101));
 assert(!box.enqueue("1|101|2|b|select|0",101));
 auto items=box.drain();
 assert(items.size()==RemoteReviewMailbox::capacity);
 assert(items.front().trustedEpoch==101);
 assert(box.drain().empty());
 assert(box.enqueue("1|102|1|c|select|0",102));
 box.clear();
 assert(box.drain().empty());
}
