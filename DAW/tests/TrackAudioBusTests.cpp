#include "TrackAudioBus.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{
    TrackAudioBus bus;require(!bus.prepare(0) && bus.prepare(64),"prepare limits");
    juce::AudioBuffer<float> a(2,64),b(2,64),out(2,80);a.clear();b.clear();out.clear();
    for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i){a.setSample(ch,i,0.2f);b.setSample(ch,i,0.3f);}
    require(!bus.begin(65) && bus.begin(32),"block bounds");
    require(bus.add(a,0,0,32) && bus.add(b,8,0,32),"sum distinct sources");
    require(!bus.add(a,-1,0,4) && !bus.add(a,0,31,2),"window bounds");
    int calls=0;
    const float peak=bus.mix(out,7,0.5f,-1,[&](juce::AudioBuffer<float>& summed){
        ++calls;require(summed.getNumSamples()==32,"partial callback view length");
        require(std::abs(summed.getSample(0,0)-0.5f)<1e-6,"effect did not receive summed sources");
        summed.applyGain(0.5f);return true;
    });
    require(calls==1 && std::abs(peak-0.125f)<1e-6 && out.getSample(0,7)==0.125f &&
        out.getSample(1,7)==0 && out.getSample(0,6)==0 && out.getSample(0,39)==0,"processing/gain/pan/offset");
    require(bus.begin(8) && bus.add(a,0,3,2),"clip window");out.clear();
    bus.mix(out,0,1,0,[](juce::AudioBuffer<float>&){return true;});
    require(out.getSample(0,0)==0 && out.getSample(0,3)==0.2f && out.getSample(0,5)==0,"reuse retained prior track or gap");
    require(bus.begin(64) && bus.add(b,0,0,64),"full prepared block");out.clear();
    require(bus.mix(out,0,1,0,[](juce::AudioBuffer<float>&){return false;})==0 && out.getMagnitude(0,80)==0,"failed effect reached mix");
    require(bus.begin(64),"begin mono output");juce::AudioBuffer<float> mono(1,64);mono.clear();
    require(bus.add(a,0,0,64),"mono source");bus.mix(mono,0,1,0,[](juce::AudioBuffer<float>&){return true;});
    require(mono.getSample(0,63)==0.2f,"mono device output");
    // Compare dry routing against the preceding direct source-sum algorithm.
    juce::AudioBuffer<float> reference(2,80);reference.clear();out.clear();
    require(bus.begin(64) && bus.add(a,0,0,32) && bus.add(b,16,16,48),"overlapping dry windows");
    const float gains[]{0.7f*(1.0f-0.3f),0.7f};
    for(int ch=0;ch<2;++ch){reference.addFrom(ch,5,a,ch,0,32,gains[ch]);reference.addFrom(ch,21,b,ch,16,48,gains[ch]);}
    bus.mix(out,5,0.7f,0.3f,[](juce::AudioBuffer<float>&){return true;});
    for(int ch=0;ch<2;++ch)for(int i=0;i<80;++i)
        require(std::abs(out.getSample(ch,i)-reference.getSample(ch,i))<1e-6,"dry routing changed mix");
    std::cout<<"Track audio bus tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
