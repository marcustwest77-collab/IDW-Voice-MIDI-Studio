#include "FormantPitchShifter.h"
#include "ScaleAwareHarmony.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}}
int main(){using namespace idw;
    auto c=scaleAwareHarmony(60,0,2741,2);check(c.voices==2&&c.first==4&&c.second==7,"C-major third/fifth wrong");
    auto a=scaleAwareHarmony(57,9,1453,2);check(a.first==3&&a.second==7,"A-minor third/fifth wrong");
    auto stack=scaleAwareHarmony(64,0,2741,1);check(stack.first==-4&&stack.second==3,"Scale stack wrong");
    auto octaves=scaleAwareHarmony(60,0,2741,3);check(octaves.first==-12&&octaves.second==12,"Octave stack wrong");
    FormantPitchShifter shifter;shifter.prepare(48000);double energy=0;
    for(int i=0;i<96000;++i){const float carrier=(float)(.16*std::sin(6.28318530717958647692*120*i/48000.0)+.08*std::sin(6.28318530717958647692*720*i/48000.0));const auto y=shifter.process(carrier,1.5f,1.0f);check(std::isfinite(y)&&std::abs(y)<=1.6f,"Formant shifter unstable");if(i>5000)energy+=y*y;}
    check(energy>1.0,"Formant shifter produced silence");shifter.reset();
    for(int i=0;i<1000;++i){const float x=i==0?.5f:0;check(shifter.process(x,1.5f,0)==x,"Formant shifter bypass changed input");}
    std::cout<<"PASS V10.4 scale-aware harmony / LPC formant beta stability / bypass\n";
}
