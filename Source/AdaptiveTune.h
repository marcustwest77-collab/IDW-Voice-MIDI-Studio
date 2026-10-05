#pragma once
#include <algorithm>
#include <cmath>

namespace idw {

inline float adaptiveRetuneMilliseconds(float baseMs,float correctionSemitones,float preserve,bool targetChanged){
    baseMs=std::clamp(baseMs,5.0f,200.0f);preserve=std::clamp(preserve,0.0f,1.0f);
    if(targetChanged)return std::max(5.0f,baseMs*(0.22f+0.28f*(1.0f-preserve)));
    const float nearNote=1.0f-std::clamp(std::abs(correctionSemitones)/0.55f,0.0f,1.0f);
    return std::min(500.0f,baseMs*(1.0f+3.0f*preserve*nearNote));
}

inline float smoothingCoefficient(double sampleRate,float milliseconds){
    const auto seconds=std::max(0.001,static_cast<double>(milliseconds)/1000.0);
    return static_cast<float>(1.0-std::exp(-1.0/(std::max(8000.0,sampleRate)*seconds)));
}

} // namespace idw
