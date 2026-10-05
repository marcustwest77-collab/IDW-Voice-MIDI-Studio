#include "AdaptiveTune.h"
#include <iostream>
#include <stdexcept>

namespace { void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);} }
int main(){using namespace idw;
    check(adaptiveRetuneMilliseconds(80,.05f,1,false)>280,"Vibrato preserve did not slow near-note correction");
    check(adaptiveRetuneMilliseconds(80,1.2f,1,false)==80,"Large correction should retain base speed");
    check(adaptiveRetuneMilliseconds(80,.05f,1,true)<45,"Note transition did not accelerate correction");
    check(adaptiveRetuneMilliseconds(80,.05f,0,false)==80,"Disabled preserve changed base speed");
    const auto fast=smoothingCoefficient(48000,10),slow=smoothingCoefficient(48000,100);check(fast>slow&&slow>0,"Invalid smoothing coefficients");
    std::cout<<"PASS V10.3 adaptive transitions / vibrato preserve / smoothing\n";
}
