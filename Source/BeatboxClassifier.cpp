#include "BeatboxClassifier.h"
#include <cmath>
BeatboxClassifier::BeatboxClassifier(){for(auto& c:counts)c.store(0);for(auto& model:models)for(auto& v:model)v.store(0);}
void BeatboxClassifier::prepare(double s){sr=s;ring.assign((size_t)std::ceil(sr*0.023)+4,0);pos=cooldown=pending=0;previous=peak=0;training.store(-1);remaining.store(0);command.store(-1);}
BeatboxClassifier::Features BeatboxClassifier::features() const {
    fftData.fill(0);
    // Resample the last 21.3ms to a common analysis rate so profiles survive sample-rate changes.
    for(int i=0;i<1024;++i){
        const double back=(1023-i)*sr/48000.0;
        const int ringSize=(int)ring.size();
        const int a=(pos-1-(int)back+ringSize*2)%ringSize,b=(a-1+ringSize)%ringSize;
        const float fraction=(float)(back-std::floor(back));
        const float sample=ring[(size_t)a]*(1-fraction)+ring[(size_t)b]*fraction;
        fftData[(size_t)i]=sample*(0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*i/1023.0f));
    }
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    constexpr float edges[]={0,150,300,600,1200,2400,4800,9000,24000};
    Features f{};float total=1.0e-12f;
    for(int bin=1;bin<512;++bin){const float hz=bin*48000.0f/1024.0f;
        for(int band=0;band<8;++band)if(hz>=edges[band]&&hz<edges[band+1]){const float e=fftData[(size_t)bin]*fftData[(size_t)bin];f[(size_t)band]+=e;total+=e;break;}}
    for(auto& v:f)v=std::sqrt(v/total);
    return f;
}
BeatboxClassifier::Hit BeatboxClassifier::process(const float* input,int n,float threshold){
    const int cmd=command.exchange(-1);
    if(cmd>=0){pending=cooldown=0;training.store(cmd);remaining.store(5);trainingCount=0;trainingSum.fill(0);}
    else if(cmd==-2 || cmd==-3){training.store(-1);remaining.store(0);if(cmd==-3)for(auto& c:counts)c.store(0);}
    double energy=0;
    for(int i=0;i<n;++i){const float x=std::isfinite(input[i])?input[i]:0;ring[(size_t)pos]=x;pos=(pos+1)%(int)ring.size();energy+=x*x;}
    const float rms=(float)std::sqrt(energy/juce::jmax(1,n));
    cooldown=juce::jmax(0,cooldown-n);
    if(pending>0){peak=juce::jmax(peak,rms);pending-=n;
        if(pending<=0){
            const auto f=features();const int pad=training.load();
            if(pad>=0){for(int j=0;j<8;++j)trainingSum[(size_t)j]+=f[(size_t)j];++trainingCount;remaining.store(5-trainingCount);
                if(trainingCount>=5){
                    float norm=0;for(auto v:trainingSum)norm+=v*v;norm=std::sqrt(juce::jmax(norm,1.0e-9f));
                    counts[(size_t)pad].store(0);
                    for(int j=0;j<8;++j)models[(size_t)pad][(size_t)j].store(trainingSum[(size_t)j]/norm);
                    counts[(size_t)pad].store(trainingCount);training.store(-1);
                }
                previous=rms;return {};
            }
            float best=100,second=100;int winner=-1,trained=0;
            for(int p=0;p<8;++p)if(counts[(size_t)p].load()>=3){++trained;float distance=0;
                for(int j=0;j<8;++j){const float d=f[(size_t)j]-models[(size_t)p][(size_t)j].load();distance+=d*d;}
                if(distance<best){second=best;best=distance;winner=p;}else second=juce::jmin(second,distance);
            }
            if(trained==0){const float low=f[0]*f[0]+f[1]*f[1]+f[2]*f[2];const float high=f[5]*f[5]+f[6]*f[6]+f[7]*f[7];winner=low>0.55f?0:high>0.6f?2:1;}
            else if(best>0.42f || (trained>1 && second-best<0.025f))winner=-1;
            previous=rms;return {winner,juce::jlimit(1,127,(int)std::lround(20+107*std::sqrt(juce::jlimit(0.0f,1.0f,peak/0.35f))))};
        }
    } else if(cooldown==0 && rms>=threshold && rms-previous>=threshold*0.4f){
        pending=juce::jmax(1,(int)(sr*0.020));cooldown=juce::jmax(1,(int)(sr*0.070));peak=rms;
    }
    previous=rms;return {};
}
juce::ValueTree BeatboxClassifier::save() const {
    juce::ValueTree tree("DrumProfiles");
    for(int p=0;p<8;++p){juce::ValueTree pad("Pad");pad.setProperty("index",p,nullptr);pad.setProperty("count",counts[(size_t)p].load(),nullptr);
        for(int j=0;j<8;++j)pad.setProperty(juce::Identifier("f"+juce::String(j)),models[(size_t)p][(size_t)j].load(),nullptr);tree.addChild(pad,-1,nullptr);}
    return tree;
}
void BeatboxClassifier::restore(const juce::ValueTree& tree){
    command.store(-2);for(auto& c:counts)c.store(0);
    for(const auto pad:tree){const int p=pad.getProperty("index",-1);if(!juce::isPositiveAndBelow(p,8))continue;
        bool valid=true;std::array<float,8> f{};
        for(int j=0;j<8;++j){f[(size_t)j]=pad.getProperty(juce::Identifier("f"+juce::String(j)),0.0f);valid=valid&&std::isfinite(f[(size_t)j])&&f[(size_t)j]>=0&&f[(size_t)j]<=1;}
        if(valid){for(int j=0;j<8;++j)models[(size_t)p][(size_t)j].store(f[(size_t)j]);counts[(size_t)p].store(juce::jlimit(0,5,(int)pad.getProperty("count",0)));}
    }
}
