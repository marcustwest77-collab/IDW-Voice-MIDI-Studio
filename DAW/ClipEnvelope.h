#pragma once
#include "SessionModel.h"
#include <cstdint>
namespace idw::daw {
// Absolute timeline evaluation makes seeking and arbitrary block sizes identical.
// Fade-out reaches zero on the last audible sample; crossfade pairs use the same
// inclusive interval, giving complementary linear or constant-power gains.
struct ClipEnvelope {
    int64_t start=0,length=0,inFrames=0,outFrames=0;
    FadeCurve inCurve=FadeCurve::linear,outCurve=FadeCurve::linear;
    ClipEnvelope(const Clip& clip,double bpm,double rate) {
        const double scale=60.0/bpm*rate;
        start=(int64_t)std::llround(clip.startBeat*scale);
        length=(int64_t)std::llround((clip.startBeat+clip.lengthBeats)*scale)-start;
        // Round absolute fade boundaries to align pairs at fractional beats.
        inFrames=std::clamp((int64_t)std::llround((clip.startBeat+clip.fadeInBeats)*scale)-start,int64_t(0),length);
        outFrames=std::clamp(start+length-(int64_t)std::llround((clip.startBeat+clip.lengthBeats-clip.fadeOutBeats)*scale),int64_t(0),length);
        inCurve=clip.fadeInCurve;outCurve=clip.fadeOutCurve;
    }
    static float curve(double value,FadeCurve shape) noexcept {
        value=std::clamp(value,0.0,1.0);
        return (float)(shape==FadeCurve::equalPower ? std::sin(value*1.5707963267948966) : value);
    }
    float gain(int64_t frame) const noexcept {
        const auto local=frame-start;
        if(local<0 || local>=length)return 0;
        float value=1;
        if(inFrames>0 && local<inFrames)
            value*=curve(inFrames==1?1.0:(double)local/(inFrames-1),inCurve);
        if(outFrames>0 && local>=length-outFrames)
            value*=curve(outFrames==1?0.0:(double)(length-1-local)/(outFrames-1),outCurve);
        return value;
    }
    void apply(float* samples,int count,int64_t firstFrame) const noexcept {
        for(int i=0;i<count;++i)samples[i]*=gain(firstFrame+i);
    }
};
}
