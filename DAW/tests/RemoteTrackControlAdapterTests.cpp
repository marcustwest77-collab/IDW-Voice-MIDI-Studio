#include "../RemoteTrackControlAdapter.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
using namespace idw::daw::remote;
int main() {
 int selected=-1,armed=-1;
 TrackControlAdapter adapter(4,[&](std::size_t index){selected=static_cast<int>(index);return true;},
                           [&](std::size_t index){armed=static_cast<int>(index);return true;});
 CommandMessage command;command.action=Action::select_track;command.trackIndex=3;
 assert(adapter.apply(command) && selected==3);
 command.action=Action::arm_track;command.trackIndex=1;
 assert(adapter.apply(command) && armed==1);
 command.trackIndex=4;assert(!adapter.apply(command) && armed==1);
 command.trackIndex=0;command.action=Action::start_recording;
 assert(!adapter.apply(command));
 command.action=Action::delete_track;assert(!adapter.apply(command));
 TrackControlAdapter noCallbacks(3,{},{});command.action=Action::arm_track;
 assert(!noCallbacks.apply(command));
}
