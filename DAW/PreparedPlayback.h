#pragma once
#include "SessionModel.h"
#include <utility>
namespace idw::daw {
struct PreparedPlayback {
    Session session;
    std::optional<PlaybackLoopFrames> loop;
    std::int64_t firstFrame=0;
};
// Message-thread preparation. Recording deliberately gets a linear snapshot,
// without changing the saved loop preference in the editable session.
inline std::optional<PreparedPlayback> preparePlayback(const Session& session,double rate,bool allowLoop){
    if(!validPlaybackLoop(session.playbackLoop))return {};
    PreparedPlayback result{session,{},0};
    if(allowLoop && session.playbackLoopEnabled){
        result.loop=playbackLoopFrames(session.playbackLoop,session.bpm,rate);
        if(!result.loop)return {};
        result.firstFrame=result.loop->start;
    }
    return result;
}
template<class Render>
inline std::optional<std::int64_t> renderPreparedPlayback(const PreparedPlayback& playback,bool playing,
    std::int64_t cursor,int count,Render&& render){
    if(!playing)return cursor;
    return renderPlaybackSegments(cursor,count,playback.loop,std::forward<Render>(render));
}
}
