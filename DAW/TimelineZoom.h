#pragma once
#include <algorithm>
#include <cmath>

namespace idw::daw {
// View-only zoom: never changes clip beats, source offsets, or audio playback.
// Five discrete scales keep ruler, hit testing and drag distance in sync.
inline constexpr int defaultTimelineZoom=2;
inline constexpr int maxTimelineZoom=4;
inline double timelinePixelsPerBeat(int level) {
    constexpr double scales[]={6.0,9.0,12.0,18.0,24.0};
    return scales[std::clamp(level,0,maxTimelineZoom)];
}
inline double timelineBeatAtPixel(double x,double left,int level) {
    return std::max(0.0,(x-left)/timelinePixelsPerBeat(level));
}
inline double timelinePixelAtBeat(double beat,double left,int level) {
    return left+beat*timelinePixelsPerBeat(level);
}
}
