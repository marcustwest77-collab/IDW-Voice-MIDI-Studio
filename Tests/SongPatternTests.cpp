#include "SongPattern.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    idw::SongPattern pattern;pattern.bars=4;
    for(int bpm:{40,83,120,137,240})for(double rate:{44100.,48000.,96000.}){
        pattern.bpm=bpm;check(pattern.valid(),"Valid pattern rejected");
        check(std::abs(pattern.samples(rate)/rate-960.0/bpm)<1/rate,"Song duration drift");
        for(int step=1;step<64;++step){const auto boundary=(std::int64_t)std::ceil(step*rate*15/bpm);check(pattern.stepAt(boundary,rate)==step,"Step boundary is late");check(pattern.stepAt(boundary-1,rate)==step-1,"Step fired early");}
    }
    pattern.bpm=40;pattern.bars=64;check(!pattern.valid(),"Oversized song accepted");
    pattern.bpm=120;pattern.bars=64;check(pattern.valid(),"64-bar song rejected");
    pattern.bpm=0;check(!pattern.valid(),"Zero tempo accepted");
    std::cout<<"PASS song sample-clock timing / fractional boundaries / duration limits\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
