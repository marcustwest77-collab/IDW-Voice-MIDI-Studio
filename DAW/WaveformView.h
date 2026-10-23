#pragma once
#include "SessionModel.h"
#include <cmath>
#include <optional>

namespace idw::daw {
// The thumbnail displays the clip's original source interval, not its timeline
// start. Split and trim edits change sourceOffsetSeconds without touching WAV data.
// Keep the full requested interval (even past EOF) so blank source tails remain
// correctly scaled instead of stretching the available audio.
struct WaveformWindow { double startSeconds, endSeconds; };
inline std::optional<WaveformWindow> clipWaveformWindow(const Clip& clip,double bpm) {
    if(!std::isfinite(bpm) || bpm<=0 ||
       !std::isfinite(clip.startBeat) || clip.startBeat<0 ||
       !std::isfinite(clip.lengthBeats) || clip.lengthBeats<=0 ||
       !std::isfinite(clip.sourceOffsetSeconds) || clip.sourceOffsetSeconds<0)
        return std::nullopt;
    const double seconds=clip.lengthBeats*60.0/bpm;
    const double end=clip.sourceOffsetSeconds+seconds;
    if(!std::isfinite(seconds) || !std::isfinite(end) ||
       end<=clip.sourceOffsetSeconds)return std::nullopt;
    return WaveformWindow{clip.sourceOffsetSeconds,end};
}
}
