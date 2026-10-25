#pragma once
#include "SessionModel.h"
#include "TimelineMath.h"
#include <cstdint>
namespace idw::daw {
// Absolute timeline evaluation makes seeking and arbitrary block sizes identical.
// Fade-out reaches zero on the last audible sample; crossfade pairs use the same
// inclusive interval, giving complementary linear or constant-power gains.
struct ClipEnvelope {
    int64_t start=0,length=0,inFrames=0,outFrames=0;
    FadeCurve inCurve=FadeCurve::linear,outCurve=FadeCurve::linear;
    float clipGain=1.0f;
    bool valid=false;
    ClipEnvelope(const Clip& clip,double bpm,double rate) {
        int64_t end=0,fadeInEnd=0,fadeOutStart=0;
        if(!validClipGain(clip.gainDb) || !std::isfinite(clip.fadeInBeats) || !std::isfinite(clip.fadeOutBeats) ||
           clip.fadeInBeats<0 || clip.fadeOutBeats<0 ||
           clip.fadeInBeats>clip.lengthBeats || clip.fadeOutBeats>clip.lengthBeats ||
           !validFadeCurve(clip.fadeInCurve) || !validFadeCurve(clip.fadeOutCurve) ||
           !clipFrameBounds(clip.startBeat,clip.lengthBeats,bpm,rate,start,end) ||
           !frameAtBeat(clip.startBeat+clip.fadeInBeats,bpm,rate,fadeInEnd) ||
           !frameAtBeat(clip.startBeat+clip.lengthBeats-clip.fadeOutBeats,bpm,rate,fadeOutStart))return;
        length=end-start;
        inFrames=std::clamp(fadeInEnd-start,int64_t(0),length);
        outFrames=std::clamp(end-fadeOutStart,int64_t(0),length);
        inCurve=clip.fadeInCurve;outCurve=clip.fadeOutCurve;
        clipGain=(float)std::pow(10.0,clip.gainDb/20.0);
        valid=true;
    }
    static float curve(double value,FadeCurve shape) noexcept {
        value=std::clamp(value,0.0,1.0);
        return (float)(shape==FadeCurve::equalPower ? std::sin(value*1.5707963267948966) : value);
    }
    float gain(int64_t frame) const noexcept {
        if(!valid || frame<start)return 0;
        const auto local=frame-start;
        if(local>=length)return 0;
        float value=clipGain;
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
