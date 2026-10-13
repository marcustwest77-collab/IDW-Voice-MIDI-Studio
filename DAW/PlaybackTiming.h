#pragma once
#include "SessionModel.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace idw::daw {
inline double playbackBeat(std::int64_t frame,double rate,double bpm){
    if(frame<0 || !std::isfinite(rate) || rate<=0 ||
       !std::isfinite(bpm) || bpm<=0)return 0.0;
    const double beat=(double)frame*bpm/(rate*60.0);
    return std::isfinite(beat)?beat:0.0;
}
inline double arrangementEndBeat(const Session& session){
    double end=0.0;
    for(const auto& track:session.tracks){
        for(const auto& clip:track.clips)
            if(std::isfinite(clip.startBeat) && std::isfinite(clip.lengthBeats) &&
               clip.startBeat>=0 && clip.lengthBeats>0 &&
               std::isfinite(clip.startBeat+clip.lengthBeats))
                end=std::max(end,clip.startBeat+clip.lengthBeats);
        for(const auto& note:track.notes)
            if(std::isfinite(note.startBeat) && std::isfinite(note.lengthBeats) &&
               note.startBeat>=0 && note.lengthBeats>0 &&
               std::isfinite(note.startBeat+note.lengthBeats))
                end=std::max(end,note.startBeat+note.lengthBeats);
    }
    return end;
}
}
