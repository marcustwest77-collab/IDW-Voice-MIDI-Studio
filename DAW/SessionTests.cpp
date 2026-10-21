#include "SessionModel.h"
#include <stdexcept>
int main() {
    idw::daw::Session s;
    if(s.tracks.size()!=0 || s.bpm!=120.0)throw std::runtime_error("initial session");
    if(!s.addTrack(idw::daw::TrackType::audio)||!s.addTrack(idw::daw::TrackType::midi))throw std::runtime_error("add");
    if(s.tracks[0].name!="Audio 1" || s.tracks[1].name!="MIDI 2")throw std::runtime_error("names");
    if(!s.renameTrack(0,"Lead Vocal") || s.tracks[0].name!="Lead Vocal")throw std::runtime_error("rename audio");
    if(!s.renameTrack(1,"Keys / Chorus") || s.tracks[1].name!="Keys / Chorus")throw std::runtime_error("rename MIDI");
    for(const auto& invalid:{std::string{},std::string("   "),std::string("bad\nname"),std::string(257,'a')})
        if(s.renameTrack(0,invalid) || s.tracks[0].name!="Lead Vocal")throw std::runtime_error("invalid rename mutated track");
    if(s.renameTrack(99,"Missing"))throw std::runtime_error("invalid rename index");
    if(!s.setTrackMix(0,1.5f,-0.5f) || s.tracks[0].gain!=1.5f || s.tracks[0].pan!=-0.5f)throw std::runtime_error("track mix");
    if(!s.setTrackMix(0,5.0f,-5.0f) || s.tracks[0].gain!=2.0f || s.tracks[0].pan!=-1.0f)throw std::runtime_error("mix clamp");
    if(s.setTrackMix(99,1.0f,0.0f))throw std::runtime_error("invalid track accepted");
    s.setTempo(0);if(s.bpm!=20)throw std::runtime_error("low bpm");
    s.setTempo(500);if(s.bpm!=300)throw std::runtime_error("high bpm");
    if(!s.addClip(0, {"demo.wav",4.0,8.0,0.0}))throw std::runtime_error("add audio clip");
    if(s.addClip(1, {"bad.wav",0.0,1.0,0.0}))throw std::runtime_error("midi clip rejected");
    if(s.addClip(0, {"",0.0,1.0,0.0}) || s.addClip(0, {"bad.wav",-1.0,1.0,0.0}) ||
       s.addClip(0, {"bad.wav",0.0,0.0,0.0}))throw std::runtime_error("invalid clip accepted");
    if(s.tracks[0].clips.size()!=1 || s.tracks[0].clips[0].startBeat!=4.0)throw std::runtime_error("clip metadata");
    if(!s.removeTrack(0)||s.removeTrack(99)||s.tracks.size()!=1)throw std::runtime_error("remove");
}
