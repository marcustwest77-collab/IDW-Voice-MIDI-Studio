#pragma once
#include "SessionPersistence.h"
#include "SessionHistory.h"
namespace idw::daw {
inline std::optional<double> parseClipSplitBeat(const juce::String& input){
    const auto text=input.trim();
    if(text.isEmpty() || text.length()>24 || !text.containsOnly("0123456789."))return {};
    int dots=0,digits=0;for(auto c:text){if(c=='.')++dots;else ++digits;}
    if(dots>1 || digits==0)return {};
    const double beat=text.getDoubleValue();
    if(!std::isfinite(beat) || beat<0 || beat>1000000)return {};
    return beat;
}
class ClipSplitPlan {
public:
    ClipSplitPlan(const Session& session,const juce::File& file,size_t trackIndex,size_t clipIndex)
        :before(juce::JSON::toString(sessionToJson(session),true)),origin(file),track(trackIndex),clip(clipIndex){}
    bool commit(Session& session,ClipEditHistory& history,const juce::File& file,bool idle,
                const juce::String& text,juce::String& error){
        error.clear();
        if(used || !idle || file!=origin || juce::JSON::toString(sessionToJson(session),true)!=before){
            error="The project changed or is busy. Open Split audio clip again.";return false;
        }
        if(track>=session.tracks.size() || session.tracks[track].type!=TrackType::audio ||
           clip>=session.tracks[track].clips.size()){
            error="Choose an audio clip to split.";return false;
        }
        const auto beat=parseClipSplitBeat(text);
        if(!beat){error="Enter a decimal beat position between 0 and 1000000.";return false;}
        auto next=session;
        if(!next.splitClip(track,clip,*beat)){
            error="Split strictly inside the clip, outside its fade regions.";return false;
        }
        if(!history.record(session,next)){error="The split could not be added to Undo history.";return false;}
        session.tracks[track].clips.swap(next.tracks[track].clips);used=true;return true;
    }
private:
    juce::String before;juce::File origin;size_t track,clip;bool used=false;
};
}
