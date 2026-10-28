#include "../RemoteTrustedTransportEventQueue.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
 TrustedTransportEventQueue queue;
 using Kind=TrustedTransportEvent::Kind;
 assert(!queue.push({Kind::connected,41,false,true,true,{}}));
 assert(!queue.push({Kind::connected,41,true,false,true,{}}));
 assert(!queue.push({Kind::command,41,false,false,false,{}}));
 assert(queue.push({Kind::connected,41,true,true,false,{}}));
 assert(queue.push({Kind::command,41,false,false,false,"1|41|1|request|select|0"}));
 assert(queue.push({Kind::disconnected,41,false,false,false,{}}));
 auto events=queue.drain();
 assert(events.size()==3);
 assert(events[0].kind==Kind::connected && !events[0].trackControlGranted);
 assert(events[1].kind==Kind::command && events[1].epoch==41);
 assert(events[2].kind==Kind::disconnected);
 assert(queue.drain().empty());
 for(std::size_t i=0;i<TrustedTransportEventQueue::capacity;++i)
   assert(queue.push({Kind::authenticationLost,42,false,false,false,{}}));
 assert(!queue.push({Kind::authenticationLost,42,false,false,false,{}}));
 queue.clear();
 assert(queue.drain().empty());
}
