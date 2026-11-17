#pragma once
#include "ClipSplitPlan.h"
namespace idw::daw {
class ClipPositionPlan {
public:
    enum class Operation { move, trimLeft, trimRight };
    enum class Result { applied, unchanged, rejected };
    ClipPositionPlan(const Session& session,const juce::File& file,size_t trackIndex,size_t clipIndex)
        :before(juce::JSON::toString(sessionToJson(session),true)),origin(file),track(trackIndex),clip(clipIndex){}
    Result commit(Session& session,ClipEditHistory& history,const juce::File& file,bool idle,
                  Operation operation,const juce::String& text,juce::String& error){
        error.clear();
        if(used || !idle || file!=origin || juce::JSON::toString(sessionToJson(session),true)!=before){
            error="The project changed or is busy. Open Move or trim audio clip again.";return Result::rejected;
        }
        if(track>=session.tracks.size() || session.tracks[track].type!=TrackType::audio || clip>=session.tracks[track].clips.size()){
            error="Choose an audio clip.";return Result::rejected;
        }
        const auto beat=parseClipSplitBeat(text);
        if(!beat){error="Enter a decimal beat position between 0 and 1000000.";return Result::rejected;}
        auto next=session;bool valid=false;
        switch(operation){
            case Operation::move: valid=next.moveClip(track,clip,*beat);break;
            case Operation::trimLeft: valid=next.trimClipLeft(track,clip,*beat);break;
            case Operation::trimRight: valid=next.trimClipRight(track,clip,*beat);break;
        }
        if(!valid){error="Trim inside the current clip, keeping a positive length. Trimming cannot extend audio.";return Result::rejected;}
        if(sameClips(session.tracks[track].clips,next.tracks[track].clips)){used=true;return Result::unchanged;}
        if(!history.record(session,next)){error="The edit could not be added to Undo history.";return Result::rejected;}
        session.tracks[track].clips.swap(next.tracks[track].clips);used=true;return Result::applied;
    }
private:
    juce::String before;juce::File origin;size_t track,clip;bool used=false;
};
}
