#pragma once
#include <cmath>
#include <cstdint>
namespace idw {
// Absolute sample positions avoid accumulating fractional beat-rounding errors.
inline std::int64_t songBeatAt(std::int64_t sample,double rate,int bpm){return static_cast<std::int64_t>(std::floor(sample*bpm/(60.0*rate)));}
inline std::int64_t countInSamples(int bars,double rate,int bpm){return static_cast<std::int64_t>(std::ceil(bars*4.0*60.0*rate/bpm));}
class SongMetronome {
public:
    void reset(){lastBeat=-1;age=1000000;phase=0;}
    float sample(std::int64_t at,double rate,int bpm,float gain){
        const auto beat=songBeatAt(at,rate,bpm);
        if(beat!=lastBeat){lastBeat=beat;age=0;phase=0;accent=beat%4==0;}
        const double duration=rate*.025;
        if(age>=duration)return 0;
        // Short decaying cue, accented at the first beat of each 4/4 bar.
        phase+=6.283185307179586*(accent?1600.0:1100.0)/rate;
        const float envelope=static_cast<float>(std::exp(-6.0*age/duration)*(1.0-age/duration));++age;
        return std::sin(static_cast<float>(phase))*envelope*gain*(accent?1.f:.65f);
    }
private:
    std::int64_t lastBeat=-1;int age=1000000;double phase=0;bool accent=false;
};
}
