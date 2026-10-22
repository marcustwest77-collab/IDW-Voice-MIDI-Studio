#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
namespace idw {
struct LaneSettings {
    double start=0,trimIn=0,trimOut=-1,fadeIn=0,fadeOut=0;float gain=1,pan=0;bool mute=false,solo=false,clipMuted=false;
    bool valid(double duration)const{
        return std::isfinite(start)&&std::isfinite(trimIn)&&std::isfinite(trimOut)&&std::isfinite(gain)&&std::isfinite(pan)
            &&std::isfinite(fadeIn)&&std::isfinite(fadeOut)&&fadeIn>=0&&fadeOut>=0&&fadeIn+fadeOut<=((trimOut<0?duration:trimOut)-trimIn)+1e-9
            &&start>=0&&start<=180&&trimIn>=0&&trimIn<=duration&&((trimOut==-1)||(trimOut>=trimIn&&trimOut<=duration))&&gain>=0&&gain<=2&&pan>=-1&&pan<=1;
    }
    bool isDefault()const{return start==0&&trimIn==0&&trimOut==-1&&fadeIn==0&&fadeOut==0&&gain==1&&pan==0&&!mute&&!solo&&!clipMuted;}
    bool operator==(const LaneSettings& x)const{return start==x.start&&trimIn==x.trimIn&&trimOut==x.trimOut&&fadeIn==x.fadeIn&&fadeOut==x.fadeOut&&gain==x.gain&&pan==x.pan&&mute==x.mute&&solo==x.solo&&clipMuted==x.clipMuted;}
    double length(double duration)const{return (trimOut<0?duration:trimOut)-trimIn;}
    float envelope(double local,double length)const{
        double value=1;if(fadeIn>0)value=std::min(value,local/fadeIn);if(fadeOut>0)value=std::min(value,(length-local)/fadeOut);
        return (float)std::clamp(value,0.,1.);
    }
    float channelGain(int channel)const{return gain*(channel==0?std::min(1.f,1.f-pan):std::min(1.f,1.f+pan));}
};
struct SongLane {
    LaneSettings settings;
    // Buffers are immutable after import/recording finishes. Undo/recovery share them.
    std::shared_ptr<juce::AudioBuffer<float>> audio;
    double rate=48000;int frames=0;
    double duration()const{return frames/rate;}
    float sample(int channel,double time)const{
        if(settings.clipMuted||!audio||frames==0)return 0;
        const double source=time-settings.start+settings.trimIn,end=settings.trimOut<0?duration():settings.trimOut;
        if(time<settings.start||source<settings.trimIn||source>=end)return 0;
        const double location=source*rate;const int index=(int)location;if(index<0||index>=frames)return 0;
        const float a=audio->getSample(channel,index),b=index+1<frames?audio->getSample(channel,index+1):0;
        return (a+(b-a)*(float)(location-index))*settings.channelGain(channel)*settings.envelope(time-settings.start,settings.length(duration()));
    }
};
}
