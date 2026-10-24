#pragma once
#include <algorithm>
#include <cmath>

namespace idw::daw {
// UI-only quantization for timeline mouse gestures. Off does not alter the
// dragged beat position; model validation still enforces legal clip lengths.
enum class SnapGrid { quarterBeat=0, sixteenthBeat=1, oneBeat=2, off=3 };
inline SnapGrid nextSnapGrid(SnapGrid mode) {
    return static_cast<SnapGrid>((static_cast<int>(mode)+1)%4);
}
inline double snapGridStep(SnapGrid mode) {
    switch(mode){
        case SnapGrid::quarterBeat:return 0.25;
        case SnapGrid::sixteenthBeat:return 0.0625;
        case SnapGrid::oneBeat:return 1.0;
        case SnapGrid::off:return 0.0;
    }
    return 0.25;
}
inline const char* snapGridLabel(SnapGrid mode) {
    switch(mode){
        case SnapGrid::quarterBeat:return "1/4";
        case SnapGrid::sixteenthBeat:return "1/16";
        case SnapGrid::oneBeat:return "1 BEAT";
        case SnapGrid::off:return "OFF";
    }
    return "1/4";
}
inline double snapTimelineBeat(double beats,SnapGrid mode) {
    if(!std::isfinite(beats))return 0.0;
    const double clamped=std::max(0.0,beats);
    const double step=snapGridStep(mode);
    return step>0.0 ? std::round(clamped/step)*step : clamped;
}
}
