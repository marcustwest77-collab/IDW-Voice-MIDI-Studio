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
inline double timelineBeatAtPixel(double x,double left,int level,double scrollBeats=0.0) {
    return std::max(0.0,scrollBeats+(x-left)/timelinePixelsPerBeat(level));
}
inline double timelinePixelAtBeat(double beat,double left,int level,double scrollBeats=0.0) {
    return left+(beat-scrollBeats)*timelinePixelsPerBeat(level);
}
// Content bounds are expressed in beats, independent of the audio sample rate.
inline double timelineMaxScroll(double lastClipEndBeat,double viewportWidth,int level) {
    if(!std::isfinite(lastClipEndBeat) || lastClipEndBeat<=0 ||
       !std::isfinite(viewportWidth) || viewportWidth<=0)return 0.0;
    const double visibleBeats=viewportWidth/timelinePixelsPerBeat(level);
    return std::max(0.0,lastClipEndBeat+4.0-visibleBeats);
}
inline double timelineClampScroll(double requested,double lastClipEndBeat,
                                  double viewportWidth,int level) {
    if(!std::isfinite(requested))return 0.0;
    return std::clamp(requested,0.0,timelineMaxScroll(lastClipEndBeat,viewportWidth,level));
}
}
