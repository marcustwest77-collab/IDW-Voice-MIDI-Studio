#pragma once
#include "SessionPersistence.h"
namespace idw::daw {
inline std::optional<double> parsePlaybackLoopBeat(const juce::String& text){
    const auto value=text.trim();
    if(value.isEmpty() || value.length()>24 || !value.containsOnly("0123456789."))return {};
    int dots=0,digits=0;
    for(auto c:value){if(c=='.')++dots;else ++digits;}
    if(dots>1 || digits==0)return {};
    const double number=value.getDoubleValue();
    return std::isfinite(number) && number>=0 && number<=1000000?std::optional<double>{number}:std::nullopt;
}
class PlaybackLoopSettingsPlan {
public:
    enum class Result {changed,unchanged,invalid,unavailable};
    PlaybackLoopSettingsPlan(const Session& session,const juce::File& path)
        :before(juce::JSON::toString(sessionToJson(session),true)),origin(path){}
    Result apply(Session& session,const juce::File& path,bool idle,bool enabled,const juce::String& startText,const juce::String& endText){
        if(used || !idle || path!=origin || juce::JSON::toString(sessionToJson(session),true)!=before)return Result::unavailable;
        const auto start=parsePlaybackLoopBeat(startText),end=parsePlaybackLoopBeat(endText);
        if(!start || !end || !validPlaybackLoop({*start,*end}))return Result::invalid;
        used=true;
        if(session.playbackLoopEnabled==enabled && session.playbackLoop.startBeat==*start && session.playbackLoop.endBeat==*end)return Result::unchanged;
        session.playbackLoopEnabled=enabled;session.playbackLoop={*start,*end};return Result::changed;
    }
private:
    juce::String before;juce::File origin;bool used=false;
};
}
