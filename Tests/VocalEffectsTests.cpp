#include "VocalEffects.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace { void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);} }

int main(){using namespace idw;
    VocalEffects rack;rack.prepare(48000.0);VocalEffectsSettings s;
    for(int i=0;i<2000;++i){const float x=.25f*std::sin(2.0*3.14159265358979323846*220.0*i/48000.0);const auto y=rack.process(x,s);check(y.left==x&&y.right==x,"Rack bypass changed audio");}
    s.enabled=true;s.deEsserEnabled=s.compressorEnabled=s.saturationEnabled=true;
    s.doublerEnabled=s.reverbEnabled=s.delayEnabled=true;s.mix=1.0f;
    double inputEnergy=0,outputEnergy=0,stereoDifference=0;
    for(int i=0;i<96000;++i){const float x=.45f*std::sin(2.0*3.14159265358979323846*220.0*i/48000.0);const auto y=rack.process(x,s);check(std::isfinite(y.left)&&std::isfinite(y.right),"Rack produced invalid output");check(std::abs(y.left)<=1.0f&&std::abs(y.right)<=1.0f,"Rack output exceeded limiter");inputEnergy+=x*x;outputEnergy+=y.left*y.left+y.right*y.right;stereoDifference+=std::abs(y.left-y.right);}
    check(inputEnergy>0&&outputEnergy>0,"Rack produced silence");check(stereoDifference>1.0,"Stereo effects produced identical channels");
    rack.reset();s={};s.enabled=true;s.mix=0.0f;
    for(int i=0;i<100;++i){const float x=(i==0?.5f:0.0f);const auto y=rack.process(x,s);check(y.left==x&&y.right==x,"Zero mix did not preserve dry signal");}
    rack.reset();s={};s.enabled=true;s.delayEnabled=true;s.delay=1;s.delaySeconds=.1f;s.mix=1;
    float early=0,echo=0;for(int i=0;i<5000;++i){const auto y=rack.process(i==0?.8f:0.0f,s);if(i>100&&i<4700)early=std::max(early,std::abs(y.left));if(i>=4798&&i<=4802)echo=std::max(echo,std::abs(y.left));}
    check(early<.001f&&echo>.1f,"Tempo delay did not use requested time");
    rack.reset();s={};s.enabled=true;s.saturationEnabled=true;s.saturation=.8f;s.autoGain=true;s.mix=1;
    for(int i=0;i<96000;++i){const float x=.2f*std::sin(2.0*3.14159265358979323846*220.0*i/48000.0);const auto y=rack.process(x,s);check(std::isfinite(y.left)&&std::abs(y.left)<=1,"Level match produced invalid output");}
    std::cout<<"PASS V10.3 vocal rack bypass / stability / stereo / dry mix / tempo delay / level match\n";
}
