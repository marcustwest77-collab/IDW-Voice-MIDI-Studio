#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace idw::daw {
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
    const auto toFrames=[&](double seconds)->int64_t {
        return static_cast<int64_t>(std::llround(seconds*rate));
    };
    const int64_t clipStart=toFrames(startBeat*60.0/bpm);
    const int64_t clipEnd=toFrames((startBeat+lengthBeats)*60.0/bpm);
    const int64_t from=std::max(blockStart,clipStart);
    const int64_t until=std::min(blockStart+blockFrames,clipEnd);
    if(until<=from)return {};
    return {static_cast<int>(from-blockStart),static_cast<int>(until-from),
            toFrames(sourceOffsetSeconds)+(from-clipStart)};
}
}
