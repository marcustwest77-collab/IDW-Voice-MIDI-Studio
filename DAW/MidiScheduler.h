#pragma once
#include "SessionModel.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace idw::daw {
struct ScheduledMidiEvent {
    double beat=0.0;
    int pitch=60,velocity=0;
    bool noteOn=false;
    size_t trackIndex=0;
};
// Build a deterministic note-on/off event list from the piano-roll data.
// A future audio/MIDI output engine can consume these events in sample blocks.
inline std::vector<ScheduledMidiEvent> buildMidiSchedule(const Session& session) {
    std::vector<ScheduledMidiEvent> result;
    for(size_t index=0;index<session.tracks.size();++index){
        const auto& track=session.tracks[index];
        if(track.type!=TrackType::midi || track.mute)continue;
        bool anySolo=false;
        for(const auto& t:session.tracks)if(t.solo){anySolo=true;break;}
        if(anySolo && !track.solo)continue;
        for(const auto& n:track.notes){
            if(n.pitch<0 || n.pitch>127 || n.velocity<1 || n.velocity>127 ||
               !std::isfinite(n.startBeat) || n.startBeat<0 ||
               !std::isfinite(n.lengthBeats) || n.lengthBeats<=0)continue;
            result.push_back({n.startBeat,n.pitch,n.velocity,true,index});
            result.push_back({n.startBeat+n.lengthBeats,n.pitch,0,false,index});
        }
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){
        if(a.beat!=b.beat)return a.beat<b.beat;
        return !a.noteOn && b.noteOn; // note-offs first at the same beat
    });
    return result;
}
}
