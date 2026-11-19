#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace idw::daw {
// Measurements only: a boundary jump is not by itself proof of audible crackle.
struct EffectDiagnosticMetrics {
    std::uint64_t samples=0,blocks=0,nonfinite=0,overUnity=0,overDeadline=0;
    double peak=0,maxBoundaryJump=0,maxInteriorJump=0,maxProcessMs=0;
    float previous[2]{};bool havePrevious=false;
    void observe(const float* left,const float* right,int count,double processMs,double rate){
        if(!left || !right || count<=0 || !std::isfinite(rate) || rate<=0)return;
        ++blocks;samples+=(std::uint64_t)count;
        if(std::isfinite(processMs) && processMs>=0){
            maxProcessMs=std::max(maxProcessMs,processMs);
            if(processMs>1000.0*count/rate)++overDeadline;
        }
        const float* channels[]{left,right};
        for(int ch=0;ch<2;++ch){
            for(int i=0;i<count;++i){
                const double value=channels[ch][i];
                if(!std::isfinite(value)){++nonfinite;continue;}
                peak=std::max(peak,std::abs(value));if(std::abs(value)>1)++overUnity;
                if(i>0 && std::isfinite(channels[ch][i-1]))maxInteriorJump=std::max(maxInteriorJump,std::abs(value-channels[ch][i-1]));
            }
            if(havePrevious && std::isfinite(previous[ch]) && std::isfinite(channels[ch][0]))
                maxBoundaryJump=std::max(maxBoundaryJump,std::abs((double)channels[ch][0]-previous[ch]));
            previous[ch]=channels[ch][count-1];
        }
        havePrevious=true;
    }
};
}
