#pragma once

#include "VocalTuner.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace idw {

// Lightweight LPC envelope-transfer shifter. The source vocal is inverse-filtered,
// the excitation is pitch shifted, then the source envelope is re-applied. This is
// intentionally optional/beta until tuned with a broad set of real voices.
class FormantPitchShifter {
public:
    void prepare(double newRate){
        rate=std::max(8000.0,newRate);frame.assign(frameSize,0.0f);frameWrite=frameFill=hopCount=0;
        coefficients.fill(0.0f);targets.fill(0.0f);inputHistory.fill(0.0f);outputHistory.fill(0.0f);
        shifter.prepare(rate);dryDelay.prepare(rate);
    }
    void reset(){std::fill(frame.begin(),frame.end(),0.0f);frameWrite=frameFill=hopCount=0;coefficients.fill(0);targets.fill(0);inputHistory.fill(0);outputHistory.fill(0);shifter.reset();dryDelay.reset();}
    void setAnalysisHop(int samples){analysisHop=(std::size_t)std::clamp(samples,128,512);}
    int latencySamples() const{return shifter.latencySamples();}
    float process(float input,float ratio,float mix){
        input=std::isfinite(input)?input:0.0f;capture(input);
        for(std::size_t i=0;i<order;++i)coefficients[i]+=.0015f*(targets[i]-coefficients[i]);
        float residual=input;for(std::size_t i=0;i<order;++i)residual+=coefficients[i]*inputHistory[i];
        shift(inputHistory,input);
        const float shifted=shifter.process(residual,ratio,1.0f);
        float wet=shifted;for(std::size_t i=0;i<order;++i)wet-=coefficients[i]*outputHistory[i];
        if(!std::isfinite(wet)||std::abs(wet)>4.0f){outputHistory.fill(0);wet=0.0f;}else shift(outputHistory,wet);
        const float dry=dryDelay.process(input,1.0f,1.0f);mix=std::clamp(mix,0.0f,1.0f);
        if(mix<=0.0f)return input;
        wet=std::clamp(wet,-1.5f,1.5f);return dry+(wet-dry)*mix;
    }
private:
    template<std::size_t N>static void shift(std::array<float,N>& history,float value){for(std::size_t i=N-1;i>0;--i)history[i]=history[i-1];history[0]=value;}
    void capture(float value){
        frame[frameWrite]=value;frameWrite=(frameWrite+1)%frame.size();frameFill=std::min(frame.size(),frameFill+1);
        if(frameFill==frame.size()&&++hopCount>=analysisHop){hopCount=0;calculateLpc();}
    }
    void calculateLpc(){
        std::array<double,order+1> correlation{};
        for(std::size_t lag=0;lag<=order;++lag)for(std::size_t n=lag;n<frameSize;++n){
            const auto index=[this](std::size_t chronological){return (frameWrite+chronological)%frame.size();};
            const double window=.5-.5*std::cos(6.28318530717958647692*(double)n/(double)(frameSize-1));
            const double other=.5-.5*std::cos(6.28318530717958647692*(double)(n-lag)/(double)(frameSize-1));
            correlation[lag]+=(double)frame[index(n)]*(double)frame[index(n-lag)]*window*other;
        }
        if(correlation[0]<1.0e-8){targets.fill(0);return;}
        std::array<double,order+1> a{};a[0]=1.0;double error=correlation[0];
        for(std::size_t i=1;i<=order;++i){
            double sum=correlation[i];for(std::size_t j=1;j<i;++j)sum+=a[j]*correlation[i-j];
            const double reflection=std::clamp(-sum/std::max(1.0e-12,error),-.94,.94);auto previous=a;a[i]=reflection;
            for(std::size_t j=1;j<i;++j)a[j]=previous[j]+reflection*previous[i-j];
            error*=std::max(.01,1.0-reflection*reflection);
        }
        for(std::size_t i=0;i<order;++i)targets[i]=(float)a[i+1];
    }
    static constexpr std::size_t order=12,frameSize=1024;
    double rate=48000.0;std::vector<float> frame;std::size_t frameWrite=0,frameFill=0,hopCount=0,analysisHop=256;
    std::array<float,order> coefficients{},targets{},inputHistory{},outputHistory{};
    GranularPitchShifter shifter,dryDelay;
};

} // namespace idw
