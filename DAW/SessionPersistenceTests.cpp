#include "SessionPersistence.h"
#include <stdexcept>
#include <iostream>
int main(){
 using namespace idw::daw;
 Session original;original.setTempo(137);original.addTrack(TrackType::audio);
 original.tracks[0].name="Lead Vocal";
 original.tracks[0].solo=true;
 if(!original.setTrackMix(0,0.75f,-0.25f))throw std::runtime_error("mix");
 if(!original.addClip(0,{"take.wav",8.0,16.0,0.5}))throw std::runtime_error("clip");
 Session restored;
 if(!sessionFromJson(sessionToJson(original),restored))throw std::runtime_error("roundtrip");
 if(restored.bpm!=137 || restored.tracks.size()!=1 ||
    restored.tracks[0].name!="Lead Vocal" || !restored.tracks[0].solo ||
    restored.tracks[0].clips.size()!=1 || restored.tracks[0].clips[0].startBeat!=8.0 ||
    restored.tracks[0].gain!=0.75f)throw std::runtime_error("data loss");
 auto corrupt=juce::JSON::parse("{\"format\":\"wrong\",\"version\":1,\"tracks\":[]}");
 if(sessionFromJson(corrupt,restored))throw std::runtime_error("accepted invalid format");
 std::cout<<"PASS: session project persistence\n";
}
