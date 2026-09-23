#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
namespace idw {
// Fixed voice pool. Allocation only in prepare(), never during note/render calls.
class StudioSynth {
public:
    struct Settings { int lead=1,chord=3,bass=0; float gain=.55f,tone=.65f,attack=.008f,release=.25f,delay=.12f; bool mpe=false; };
    void prepare(double rate) { sr=std::max(8000.0,rate); delayLine.assign((size_t)(sr*.25),0); reset(); }
    void reset() { voices={}; bends.fill(0); ranges.fill(2); rpnMSB.fill(127);rpnLSB.fill(127);sustain.fill(false); std::fill(delayLine.begin(),delayLine.end(),0); delayPos=0; }
    void configure(Settings s) { settings=s; filterAlpha=1.0-std::exp(-6.28318530718*(150+std::clamp(s.tone,0.f,1.f)*10000)/sr); }
    void message(int status,int a,int b) {
        const int ch=status&15,type=status&240;
        a=std::clamp(a,0,127);b=std::clamp(b,0,127);
        if(type==0x90 && b>0) { noteOn(ch,a,b);return; }
        if(type==0x80 || (type==0x90 && b==0)) { for(auto& v:voices)if(v.on&&v.ch==ch&&v.note==a){v.key=false;if(!sustain[ch]&&ch!=9)release(v);}return; }
        if(type==0xe0){bends[ch]=(a+128*b-8192)/8192.0;retune(ch);return;}
        if(type!=0xb0)return;
        if(a==101)rpnMSB[ch]=b; if(a==100)rpnLSB[ch]=b;
        if(a==6&&rpnMSB[ch]==0&&rpnLSB[ch]==0){ranges[ch]=b;retune(ch);}
        if(a==64){sustain[ch]=b>=64;if(!sustain[ch])for(auto& v:voices)if(v.on&&v.ch==ch&&!v.key)release(v);}
        if(a==123)for(auto& v:voices)if(v.on&&v.ch==ch){v.key=false;release(v);}
        if(a==120){for(auto& v:voices)if(v.ch==ch)v.on=false;std::fill(delayLine.begin(),delayLine.end(),0);}
        if(a==121){sustain[ch]=false;bends[ch]=0;retune(ch);for(auto& v:voices)if(v.on&&v.ch==ch&&!v.key)release(v);}
    }
    float sample() {
        double sum=0;
        for(auto& v:voices)if(v.on){
            const double age=v.age++/sr;
            double wave=0;
            if(v.ch==9){
                rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;
                const double noise=(rng/4294967295.0)*2-1;
                if(v.note==35||v.note==36){v.phase+= (45+120*std::exp(-age*35))/sr;wave=std::sin(6.28318530718*v.phase)*std::exp(-age*10);}
                else if(v.note==38||v.note==40){v.phase+=180/sr;wave=(noise*.7+std::sin(6.28318530718*v.phase)*.3)*std::exp(-age*18);}
                else {const double high=noise-v.low;v.low=noise;wave=high*.4*std::exp(-age*(v.note==46?10:45));}
                if(age>1.2)v.on=false;
            }else{
                if(v.releasing){v.env=std::max(0.0,v.env-v.releaseStep);if(v.env==0){v.on=false;continue;}}
                else if(v.stage==0){v.env+=1/(std::max(.001f,settings.attack)*sr);if(v.env>=1){v.env=1;v.stage=1;}}
                else {v.env+= (.7-v.env)*(1-std::exp(-1/(sr*.16)));}
                v.phase+=v.increment;v.phase-=std::floor(v.phase);
                const double t=v.phase, sine=std::sin(6.28318530718*t);
                switch(v.patch){
                    case 0:wave=sine;break;
                    case 1:wave=2*t-1-blep(t,v.increment);break;
                    case 2:wave=(t<.5?1:-1)+blep(t,v.increment)-blep(std::fmod(t+.5,1.0),v.increment);break;
                    case 3:wave=(sine+.25*std::sin(12.56637061436*t))*.8;break;
                    default:wave=std::sin(6.28318530718*t+1.5*std::exp(-age*2)*std::sin(12.56637061436*t));break;
                }
                v.low+=filterAlpha*(wave-v.low);wave=v.low*v.env;
            }
            sum+=wave*v.velocity*.12;
        }
        double delayed=0;
        if(!delayLine.empty()){delayed=delayLine[delayPos];delayLine[delayPos]=(float)(sum+delayed*.28);delayPos=(delayPos+1)%delayLine.size();}
        return (float)std::tanh((sum+delayed*std::clamp(settings.delay,0.f,.6f))*std::clamp(settings.gain,0.f,1.f));
    }
    int activeVoices() const {int n=0;for(const auto& v:voices)if(v.on)++n;return n;}
private:
    struct Voice {bool on=false,key=false,releasing=false;int note=0,ch=0,patch=0,stage=0;uint64_t age=0;double phase=0,increment=0,env=0,releaseStep=0,low=0,velocity=0;};
    void release(Voice& v){if(!v.releasing){v.releasing=true;v.releaseStep=v.env/(std::max(.01f,settings.release)*sr);}}
    void tune(Voice& v){v.increment=std::min(.45,440*std::pow(2.0,(v.note-69+bends[v.ch]*ranges[v.ch])/12)/sr);}
    void retune(int ch){for(auto& v:voices)if(v.on&&v.ch==ch)tune(v);}
    void noteOn(int ch,int note,int velocity){
        Voice* chosen=nullptr;
        for(auto& v:voices)if(v.on&&v.ch==ch&&v.note==note){chosen=&v;break;}
        if(!chosen)for(auto& v:voices)if(!v.on){chosen=&v;break;}
        if(!chosen)chosen=&*std::max_element(voices.begin(),voices.end(),[](const Voice& a,const Voice& b){return a.age<b.age;});
        *chosen={};chosen->on=chosen->key=true;chosen->ch=ch;chosen->note=note;chosen->velocity=velocity/127.0;
        chosen->patch=std::clamp(!settings.mpe&&ch==1?settings.chord:!settings.mpe&&ch==2?settings.bass:settings.lead,0,4);tune(*chosen);
    }
    static double blep(double t,double dt){if(t<dt){t/=dt;return t+t-t*t-1;}if(t>1-dt){t=(t-1)/dt;return t*t+t+t+1;}return 0;}
    std::array<Voice,32> voices{};Settings settings;double sr=48000,filterAlpha=.5;
    std::array<double,16> bends{};std::array<int,16> ranges{},rpnMSB{},rpnLSB{};std::array<bool,16>sustain{};
    std::vector<float>delayLine;size_t delayPos=0;uint32_t rng=1234567;
};
}
