#include "VocalTuner.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
}

int main(){using namespace idw;
    const auto invalid=tuningDecision(0,0,.9f,.01f,.75f,0,false,0,2741,1,0);
    check(!invalid.tracking&&invalid.ratio==1,"Invalid pitch changed audio ratio");

    const auto chromatic=tuningDecision(430,.1f,.95f,.01f,.75f,0,false,0,2741,1,0);
    check(chromatic.tracking&&chromatic.targetMidi==69,"Chromatic target wrong");
    check(chromatic.correctionSemitones>.38f&&chromatic.correctionSemitones<.41f,"Chromatic correction wrong");

    const float cSharp=440.0f*std::pow(2.0f,(61.0f-69.0f)/12.0f);
    const auto scale=tuningDecision(cSharp,.1f,.95f,.01f,.75f,0,true,0,2741,1,0);
    check(scale.targetMidi==60,"Scale tie must select lower allowed note");
    const auto half=tuningDecision(430,.1f,.95f,.01f,.75f,0,false,0,2741,.5f,0);
    check(std::abs(half.correctionSemitones-chromatic.correctionSemitones*.5f)<1.0e-4f,"Amount did not scale correction");
    const auto human=tuningDecision(445,.1f,.95f,.01f,.75f,0,false,0,2741,1,1);
    check(std::abs(human.correctionSemitones)<1.0e-6f,"Humanize did not preserve small deviation");

    GranularPitchShifter shifter;shifter.prepare(48000);
    for(int i=0;i<5000;++i){const float x=std::sin(2.0*3.14159265358979323846*220.0*i/48000.0);check(shifter.process(x,1,0)==x,"True bypass changed input");}
    shifter.reset();const int total=48000;std::vector<float> shifted;
    for(int i=0;i<total;++i){
        const float x=std::sin(2.0*3.14159265358979323846*220.0*i/48000.0);
        const float y=shifter.process(x,2.0f,1.0f);
        check(std::isfinite(y)&&std::abs(y)<1.2f,"Pitch shifter produced invalid output");
        if(i>shifter.latencySamples()+3000)shifted.push_back(y);
    }
    const auto energyAt=[&](double hz){double re=0,im=0;for(std::size_t i=0;i<shifted.size();++i){const double angle=2.0*3.14159265358979323846*hz*i/48000.0;re+=shifted[i]*std::cos(angle);im-=shifted[i]*std::sin(angle);}return re*re+im*im;};
    check(energyAt(440)>energyAt(220)*8,"Pitch shifter octave energy did not move to target");
    std::cout<<"PASS V10 tuning decisions / scale quantization / humanize / bypass / audio octave shift\n";
}
