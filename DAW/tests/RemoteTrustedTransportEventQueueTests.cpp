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
 assert(events.size()==2);
 assert(events[0].kind==Kind::connected && !events[0].trackControlGranted);
 // Disconnect supersedes undelivered commands from the affected epoch.
 assert(events[1].kind==Kind::disconnected);
 assert(queue.drain().empty());
 for(std::size_t i=0;i<TrustedTransportEventQueue::capacity;++i)
   assert(queue.push({Kind::authenticationLost,42,false,false,false,{}}));
 assert(queue.push({Kind::authenticationLost,42,false,false,false,{}}));
 auto terminalEvents=queue.drain();
 assert(terminalEvents.size()==TrustedTransportEventQueue::capacity);
 assert(terminalEvents.back().kind==Kind::authenticationLost);
 for(std::size_t i=0;i<TrustedTransportEventQueue::capacity;++i)
   assert(queue.push({Kind::command,43,false,false,false,
       "1|43|"+std::to_string(i+1)+"|id"+std::to_string(i)+"|arm|0"}));
 assert(queue.push({Kind::disconnected,43,false,false,false,{}}));
 terminalEvents=queue.drain();
 assert(terminalEvents.size()==1);
 assert(terminalEvents.front().kind==Kind::disconnected);
 assert(!queue.push({Kind::command,43,false,false,false,
    "1|43|99|late|arm|0"}));
 assert(queue.push({Kind::connected,44,true,true,true,{}}));
 assert(queue.push({Kind::command,44,false,false,false,
    "1|44|1|fresh|arm|0"}));
 queue.clear();
 assert(queue.drain().empty());
}
