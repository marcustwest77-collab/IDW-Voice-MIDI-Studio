#include "../RemoteControlCommandParser.h"
#include <cassert>
using namespace idw::daw::remote;
int main(){
 CommandMessage m;
 assert(parseControlCommand("1|7|1|req_1|arm|2",m));
 assert(m.sessionEpoch==7 && m.sequence==1 && m.trackIndex==2 && m.action==Action::arm_track);
 assert(!parseControlCommand("1|7|1|req_1|delete|2",m));
 assert(!parseControlCommand("1|7|1|req_1|arm|-1",m));
 assert(!parseControlCommand("1|7|1|req_1|arm|2|extra",m));
 assert(!parseControlCommand("1|7|1|../bad|arm|2",m));
 assert(!parseControlCommand("1|7|18446744073709551616|req|arm|2",m));
 assert(!parseControlCommand("1|7|0|req|arm|2",m));
 assert(!parseControlCommand("2|7|1|req|arm|2",m));
 assert(m.sequence==1 && m.action==Action::arm_track); // rejected input does not mutate output
}
