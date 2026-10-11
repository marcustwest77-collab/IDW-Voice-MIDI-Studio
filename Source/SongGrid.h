#pragma once
#include <algorithm>
#include <cmath>
namespace idw {
// beatsPerStep == 0 disables snapping. Position bounds match Song Studio.
inline double snapSongTime(double seconds,int bpm,double beatsPerStep){
    if(!std::isfinite(seconds))return 0;
    seconds=std::clamp(seconds,0.,180.);
    if(bpm<40||bpm>240||!std::isfinite(beatsPerStep)||beatsPerStep<=0)return seconds;
    const double step=60.0/bpm*beatsPerStep;
    return std::clamp(std::round(seconds/step)*step,0.,180.);
}
}
