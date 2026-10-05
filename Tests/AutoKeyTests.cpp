#include "AutoKeyDetector.h"
#include <iostream>
#include <stdexcept>

namespace { void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);} }

int main(){using namespace idw;
    AutoKeyDetector detector;check(!detector.suggestion().valid,"Empty learner returned a key");detector.start();
    for(int repeat=0;repeat<20;++repeat)for(int note:{60,60,64,67,67,72})detector.observe(note,.95f);
    auto major=detector.suggestion();check(major.valid&&major.learning,"Active learner status missing");check(major.root==0&&major.mode==0,"C major phrase detected as wrong key");
    detector.stop();check(!detector.suggestion().learning,"Learner did not stop");
    detector.start();for(int repeat=0;repeat<20;++repeat)for(int note:{57,57,60,64,64,69})detector.observe(note,.9f);
    auto minor=detector.suggestion();check(minor.valid&&minor.root==9&&minor.mode==1,"A minor phrase detected as wrong key");
    detector.start();for(int i=0;i<23;++i)detector.observe(60,1);check(!detector.suggestion().valid,"Learner accepted too little evidence");
    std::cout<<"PASS V10.2 Auto-Key major / minor / minimum evidence / state\n";
}
