#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
namespace idw::daw {
struct PlaybackLoopRegion {double startBeat=0,endBeat=4;};
inline bool validPlaybackLoop(const PlaybackLoopRegion& region){
    return std::isfinite(region.startBeat) && std::isfinite(region.endBeat) &&
        region.startBeat>=0 && region.endBeat<=1000000 && region.endBeat-region.startBeat>=0.25;
}
struct PlaybackLoopFrames {
    std::int64_t start=0,end=0;
    int fadeFrames=1;
    bool valid() const {return start>=0 && end>start && end-start>=4 && fadeFrames>=1 &&
        fadeFrames<=128 && fadeFrames<=(end-start)/2;}
};
// Compile off the audio thread. End is exclusive; each beat boundary rounds
// once to the nearest device frame, avoiding per-cycle accumulated rounding.
inline std::optional<PlaybackLoopFrames> playbackLoopFrames(const PlaybackLoopRegion& region,double bpm,double rate){
    if(!validPlaybackLoop(region) || !std::isfinite(bpm) || bpm<20 || bpm>300 ||
       !std::isfinite(rate) || rate<8000 || rate>768000)return {};
    const double factor=rate*60.0/bpm;
    const auto start=static_cast<std::int64_t>(std::llround(region.startBeat*factor));
    const auto end=static_cast<std::int64_t>(std::llround(region.endBeat*factor));
    const auto fade=static_cast<int>(std::min<std::int64_t>({128,static_cast<std::int64_t>(rate*0.002),(end-start)/4}));
    PlaybackLoopFrames result{start,end,std::max(1,fade)};
    return result.valid()?std::optional<PlaybackLoopFrames>{result}:std::nullopt;
}
// Caller owns clearing/rendering output and the playing/stopped state. This
// helper allocates nothing, does no I/O, and never invokes render for bad input.
// Render signature: (output offset, count, absolute timeline frame).
template<class Render>
inline std::optional<std::int64_t> renderPlaybackSegments(std::int64_t cursor,int count,
    const std::optional<PlaybackLoopFrames>& loop,Render&& render){
    if(cursor<0 || count<0 || count>65536 || (loop && !loop->valid()))return {};
    if(!loop){
        if(cursor>std::numeric_limits<std::int64_t>::max()-count)return {};
        if(count>0)render(0,count,cursor);
        return cursor+count;
    }
    const auto span=loop->end-loop->start;
    if(cursor<loop->start)cursor=loop->start;
    else if(cursor>=loop->end)cursor=loop->start+(cursor-loop->start)%span;
    int written=0;
    while(written<count){
        const int frames=static_cast<int>(std::min<std::int64_t>(count-written,loop->end-cursor));
        render(written,frames,cursor);written+=frames;cursor+=frames;
        if(cursor==loop->end)cursor=loop->start;
    }
    return cursor;
}
inline float playbackLoopGain(std::int64_t frame,const PlaybackLoopFrames& loop){
    if(!loop.valid() || frame<loop.start || frame>=loop.end)return 0;
    const auto distance=std::min(frame-loop.start,loop.end-1-frame);
    return std::min(1.0f,static_cast<float>(distance)/loop.fadeFrames);
}
// Apply after the master processing of each loop segment. The short linear
// fade is deliberate: arbitrary loop endpoints are not assumed to match.
inline bool applyPlaybackLoopFade(float* left,float* right,int count,std::int64_t frame,const PlaybackLoopFrames& loop){
    if(!left || !loop.valid() || count<0 || count>65536 || frame<loop.start || frame>=loop.end || count>loop.end-frame)return false;
    for(int i=0;i<count;++i){const float gain=playbackLoopGain(frame+i,loop);left[i]*=gain;if(right && right!=left)right[i]*=gain;}
    return true;
}
}
