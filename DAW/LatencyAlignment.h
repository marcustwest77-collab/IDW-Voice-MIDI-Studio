#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace idw::daw {
// Pure timing-analysis helper for a known test pulse recorded in loopback.
// Return lag in whole samples relative to the expected pulse position.
// Positive lag: pulse arrives late. Does not modify recorded audio.
struct PulseAlignment {
    bool valid=false;
    int lagSamples=0;
    float peak=0.0f;
};
inline PulseAlignment locateLoopbackPulse(const float* recorded,std::size_t count,
                                          std::size_t expectedSample,
                                          std::size_t searchRadius,float minPeak=0.05f) {
    PulseAlignment result;
    if(!recorded || count==0 || expectedSample>=count ||
       !std::isfinite(minPeak) || minPeak<=0.0f)return result;
    const auto first=expectedSample>searchRadius?expectedSample-searchRadius:0;
    const auto last=std::min(count-1,expectedSample+std::min(searchRadius,count-1-expectedSample));
    std::size_t best=first;
    for(auto i=first;i<=last;++i){
        const float value=recorded[i];
        if(std::isfinite(value) && std::abs(value)>result.peak){
            result.peak=std::abs(value);
            best=i;
        }
    }
    if(result.peak<minPeak)return result;
    const auto delta=static_cast<double>(best)-static_cast<double>(expectedSample);
    if(delta<std::numeric_limits<int>::min() || delta>std::numeric_limits<int>::max())
        return result;
    result.valid=true;
    result.lagSamples=static_cast<int>(delta);
    return result;
}
inline double alignmentMilliseconds(int lagSamples,double sampleRate){
    return std::isfinite(sampleRate) && sampleRate>0.0
        ?1000.0*static_cast<double>(lagSamples)/sampleRate:0.0;
}
}
