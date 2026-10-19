#include "TrackVocalEffects.h"
#include "SessionModel.h"
#include <stdexcept>
#include <iostream>
int main() {
    using namespace idw::daw;
    Session s;s.addTrack(TrackType::audio);s.addTrack(TrackType::midi);
    if(s.tracks[0].vocalFxPreset!=0 || s.setTrackVocalFxPreset(0,4) ||
       s.setTrackVocalFxPreset(1,1) || s.setTrackVocalFxPreset(99,1))
        throw std::runtime_error("preset validation");
    TrackVocalEffects fx;
    const auto bypass=fx.process(0.325f,-0.23f,0);
    if(bypass.left!=0.325f || bypass.right!=-0.23f)
        throw std::runtime_error("bypass must be exact stereo");
    for(int preset=1;preset<=3;++preset){
        fx.reset();
        float first=0;bool changed=false;
        for(int i=0;i<5000;++i){
            const float input=0.5f*std::sin(i*0.13f);
            const auto out=fx.process(input,0,preset);
            if(i==1)first=out.left;
            if(!std::isfinite(out.left) || out.right!=0)
                throw std::runtime_error("finite output and stereo isolation");
            changed |= std::abs(out.left-input)>0.001f;
        }
        if(!changed)throw std::runtime_error("preset inaudible");
        fx.reset();fx.process(0,0,preset);
        if(fx.process(0.5f*std::sin(0.13f),0,preset).left!=first)
            throw std::runtime_error("restart must be repeatable");
    }
    std::cout << "Track vocal effects passed\n";
}
