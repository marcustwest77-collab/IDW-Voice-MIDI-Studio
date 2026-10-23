#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
namespace idw::daw {
// One checked conversion order for playback windows, fades and offline export.
// Changing the multiplication order near a half sample can change rounding.
inline bool roundedSampleFrame(double frames,int64_t& result) noexcept {
    if(!std::isfinite(frames) || frames<0 ||
       frames>=static_cast<double>(std::numeric_limits<int64_t>::max()))return false;
    result=static_cast<int64_t>(std::llround(frames));
    return true;
}
inline bool frameAtBeat(double beat,double bpm,double rate,int64_t& result) noexcept {
    return std::isfinite(beat) && beat>=0 && std::isfinite(bpm) && bpm>0 &&
        std::isfinite(rate) && rate>0 && roundedSampleFrame(beat*60.0/bpm*rate,result);
}
inline bool clipFrameBounds(double beat,double length,double bpm,double rate,
                            int64_t& start,int64_t& end) noexcept {
    return std::isfinite(length) && length>0 && frameAtBeat(beat,bpm,rate,start) &&
        frameAtBeat(beat+length,bpm,rate,end) && end>=start;
}
// Pure timeline calculations shared by the playback engine and UI.
// All frame positions are measured against one session clock.
struct ClipWindow {
    int offset=0;     // Offset in the current audio block.
    int frames=0;     // Audible overlap frames.
    int64_t sourceFrame=0;
};
inline ClipWindow overlap(int64_t blockStart, int blockFrames, double rate,
                          double bpm, double startBeat, double lengthBeats,
                          double sourceOffsetSeconds=0.0) noexcept {
    if(blockStart<0 || blockFrames<=0 || !std::isfinite(rate) || rate<=0 ||
       !std::isfinite(bpm) || bpm<=0 || !std::isfinite(startBeat) || startBeat<0 ||
       !std::isfinite(lengthBeats) || lengthBeats<=0 ||
       !std::isfinite(sourceOffsetSeconds) || sourceOffsetSeconds<0) return {};
    int64_t clipStart=0,clipEnd=0,sourceStart=0;
    if(!clipFrameBounds(startBeat,lengthBeats,bpm,rate,clipStart,clipEnd) ||
       !roundedSampleFrame(sourceOffsetSeconds*rate,sourceStart) ||
       blockStart>std::numeric_limits<int64_t>::max()-blockFrames)return {};
    const int64_t from=std::max(blockStart,clipStart);
    const int64_t until=std::min(blockStart+blockFrames,clipEnd);
    if(until<=from)return {};
    // Keep both the first source frame and its exclusive end representable.
    if(sourceStart>std::numeric_limits<int64_t>::max()-(until-clipStart))return {};
    return {static_cast<int>(from-blockStart),static_cast<int>(until-from),
            sourceStart+(from-clipStart)};
}
}
