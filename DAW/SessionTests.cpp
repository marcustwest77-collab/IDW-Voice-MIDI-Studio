#include "SessionModel.h"
#include <stdexcept>
int main() {
    idw::daw::Session s;
    if(s.tracks.size()!=0 || s.bpm!=120.0)throw std::runtime_error("initial session");
    if(!s.addTrack(idw::daw::TrackType::audio)||!s.addTrack(idw::daw::TrackType::midi))throw std::runtime_error("add");
    if(s.tracks[0].name!="Audio 1" || s.tracks[1].name!="MIDI 2")throw std::runtime_error("names");
    s.setTempo(0);if(s.bpm!=20)throw std::runtime_error("low bpm");
    s.setTempo(500);if(s.bpm!=300)throw std::runtime_error("high bpm");
    if(!s.removeTrack(0)||s.removeTrack(99)||s.tracks.size()!=1)throw std::runtime_error("remove");
}
