#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace idw {

struct TuningDecision {
    float measuredMidi = -1000.0f;
    int targetMidi = -1;
    float correctionSemitones = 0.0f;
    float ratio = 1.0f;
    bool tracking = false;
};

inline int positiveModulo(int value, int modulus) {
    const int result=value%modulus;
    return result<0?result+modulus:result;
}

inline bool scaleContains(int midiNote, int root, std::uint16_t mask) {
    const int relative=positiveModulo(midiNote-root,12);
    return (mask&(1u<<relative))!=0;
}

inline int nearestTuningNote(float measuredMidi, int root, std::uint16_t mask, bool songScale) {
    const int rounded=static_cast<int>(std::lround(measuredMidi));
    if(!songScale)return std::clamp(rounded,0,127);
    if(mask==0)mask=1;
    int best=std::clamp(rounded,0,127);
    float bestDistance=1000.0f;
    for(int note=0;note<128;++note){
        if(!scaleContains(note,root,mask))continue;
        const float distance=std::abs(static_cast<float>(note)-measuredMidi);
        if(distance<bestDistance-1.0e-6f || (std::abs(distance-bestDistance)<=1.0e-6f&&note<best)){
            best=note;bestDistance=distance;
        }
    }
    return best;
}

inline TuningDecision tuningDecision(float detectedHz,float rms,float confidence,
                                     float gate,float requiredConfidence,float calibrationCents,
                                     bool songScale,int root,std::uint16_t mask,
                                     float amount,float humanize) {
    TuningDecision result;
    if(!std::isfinite(detectedHz)||detectedHz<=0||rms<gate||confidence<requiredConfidence)return result;
    result.measuredMidi=69.0f+12.0f*std::log2(detectedHz/440.0f)+calibrationCents/100.0f;
    if(!std::isfinite(result.measuredMidi)||result.measuredMidi<0||result.measuredMidi>127)return result;
    result.targetMidi=nearestTuningNote(result.measuredMidi,root,mask,songScale);
    float difference=static_cast<float>(result.targetMidi)-result.measuredMidi;
    const float deadband=std::clamp(humanize,0.0f,1.0f)*0.30f;
    if(std::abs(difference)<=deadband)difference=0;
    else difference-=std::copysign(deadband,difference);
    result.correctionSemitones=std::clamp(difference*std::clamp(amount,0.0f,1.0f),-12.0f,12.0f);
    result.ratio=std::pow(2.0f,result.correctionSemitones/12.0f);
    result.tracking=true;
    return result;
}

// A causal dual-read-head pitch shifter. Each read head fades to zero before
// its delay trajectory wraps, avoiding a hard discontinuity. It is deliberately
// compact and allocation-free after prepare(), making it safe for the audio thread.
class GranularPitchShifter {
public:
    void prepare(double sampleRate) {
        rate=std::max(8000.0,sampleRate);
        minDelay=std::max(2,static_cast<int>(std::lround(rate*0.003)));
        range=std::max(32,static_cast<int>(std::lround(rate*0.022)));
        fixedDelay=minDelay+range/2;
        buffer.assign(static_cast<std::size_t>(minDelay+range+8),0.0f);
        reset();
    }
    void reset(){std::fill(buffer.begin(),buffer.end(),0.0f);writeIndex=0;filled=0;phase=0;}
    int latencySamples() const{return fixedDelay;}
    bool isReady() const{return !buffer.empty()&&filled>=buffer.size();}
    float process(float input,float ratio,float mix) {
        if(buffer.empty())return input;
        buffer[writeIndex]=std::isfinite(input)?input:0.0f;
        writeIndex=(writeIndex+1)%buffer.size();filled=std::min(buffer.size(),filled+1);
        mix=std::clamp(mix,0.0f,1.0f);
        if(mix<=0||filled<buffer.size())return input;
        ratio=std::clamp(std::isfinite(ratio)?ratio:1.0f,0.5f,2.0f);
        const float difference=ratio-1.0f;
        float wet=read(static_cast<float>(fixedDelay));
        if(std::abs(difference)>1.0e-5f){
            phase+=std::abs(difference)/static_cast<float>(range);
            phase-=std::floor(phase);
            float other=phase+0.5f;if(other>=1.0f)other-=1.0f;
            const auto delay=[&](float p){return static_cast<float>(minDelay)+(difference>0?(1.0f-p):p)*static_cast<float>(range);};
            const float w1=window(phase),w2=window(other);
            const float total=std::max(1.0e-6f,w1+w2);
            wet=(read(delay(phase))*w1+read(delay(other))*w2)/total;
        }
        const float dry=read(static_cast<float>(fixedDelay));
        return dry+(wet-dry)*mix;
    }
private:
    static float window(float p){const float s=std::sin(3.14159265358979323846f*p);return s*s;}
    float read(float delay) const {
        const float size=static_cast<float>(buffer.size());
        float position=static_cast<float>(writeIndex)-delay;
        while(position<0)position+=size;
        while(position>=size)position-=size;
        const auto a=static_cast<std::size_t>(position);
        const auto b=(a+1)%buffer.size();
        const float fraction=position-static_cast<float>(a);
        return buffer[a]+(buffer[b]-buffer[a])*fraction;
    }
    std::vector<float> buffer;
    std::size_t writeIndex=0,filled=0;
    double rate=48000.0;
    int minDelay=144,range=1056,fixedDelay=672;
    float phase=0;
};

}
