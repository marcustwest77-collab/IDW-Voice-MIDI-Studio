#pragma once
#include "SessionPersistence.h"

namespace idw::daw {
class TrackRenamePlan {
public:
    TrackRenamePlan(const Session& session,const juce::File& origin,size_t selected)
        : before(juce::JSON::toString(sessionToJson(session),true)),project(origin),track(selected),
          previous(selected<session.tracks.size()?session.tracks[selected].name:std::string{}) {}
    const std::string& previousName() const {return previous;}
    size_t trackIndex() const {return track;}
    bool commit(Session& session,const juce::File& origin,bool idle,const juce::String& input,juce::String& error){
        error.clear();
        if(!idle){error="Stop playback, recording and export before renaming a track.";return false;}
        if(used || origin!=project || track>=session.tracks.size() ||
           juce::JSON::toString(sessionToJson(session),true)!=before){
            error="The project changed. Select the track and choose Rename selected track again.";return false;
        }
        const auto name=input.trim();
        if(name.isEmpty() || name.length()>64){error="Enter a track name of 1 to 64 characters.";return false;}
        for(auto c:input)if(c<32 || (c>=127 && c<=159) || c==0x2028 || c==0x2029){
            error="Track names must fit on one line and cannot contain control characters.";return false;
        }
        session.tracks[track].name=name.toStdString();used=true;return true;
    }
private:
    juce::String before;
    juce::File project;
    size_t track;
    std::string previous;
    bool used=false;
};
}
