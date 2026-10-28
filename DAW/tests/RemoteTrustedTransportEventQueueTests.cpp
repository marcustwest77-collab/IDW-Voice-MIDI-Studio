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
 assert(events.size()==1);
 // Disconnect supersedes undelivered admission and commands for this epoch.
 assert(events.front().kind==Kind::disconnected);
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
 assert(!queue.push({Kind::command,42,false,false,false,
    "1|42|1|older|arm|0"}));
 assert(!queue.push({Kind::connected,43,true,true,true,{}}));
 assert(!queue.push({Kind::connected,42,true,true,true,{}}));
 assert(queue.push({Kind::connected,44,true,true,true,{}}));
 assert(queue.push({Kind::command,44,false,false,false,
    "1|44|1|fresh|arm|0"}));
 assert(!queue.push({Kind::command,43,false,false,false,
    "1|43|9|delayed|arm|0"}));
 queue.clear();
 assert(queue.drain().empty());
 // A rejected admission cannot advance the epoch and invalidate current control.
 TrustedTransportEventQueue saturated;
 assert(saturated.push({Kind::connected,60,true,true,true,{}}));
 for(std::size_t i=1;i<TrustedTransportEventQueue::capacity;++i)
   assert(saturated.push({Kind::command,60,false,false,false,
      "1|60|"+std::to_string(i)+"|q"+std::to_string(i)+"|arm|0"}));
 assert(!saturated.push({Kind::connected,61,true,true,true,{}}));
 saturated.drain();
 assert(saturated.push({Kind::command,60,false,false,false,"1|60|33|still_valid|arm|0"}));
 assert(saturated.drain().size()==1);
}
