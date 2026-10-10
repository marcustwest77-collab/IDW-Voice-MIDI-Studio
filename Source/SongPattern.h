#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace idw {
struct SongPattern {
    int bpm=100,bars=4;
    std::array<std::uint16_t,3> rows{{0x0101,0x1010,0x5555}};
    float beatGain=0.8f,trackGain=1.0f;
    bool valid() const {return bpm>=40&&bpm<=240&&bars>=1&&bars<=64&&bars*240.0/bpm<=180&&std::isfinite(beatGain)&&std::isfinite(trackGain)&&beatGain>=0&&beatGain<=2&&trackGain>=0&&trackGain<=2;}
    std::int64_t samples(double rate) const {return (std::int64_t)std::llround(bars*4.0*60.0*rate/bpm);}
    std::int64_t stepAt(std::int64_t sample,double rate) const {return (std::int64_t)std::floor(sample*bpm/(rate*15.0));}
};
}
