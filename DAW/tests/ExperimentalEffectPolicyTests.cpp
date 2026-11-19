#include "ExperimentalEffectPolicy.h"
#include <iostream>
#include <limits>
using namespace idw::daw;
int main(){
    if(experimentalEffectBlocksDocumentAction(false,false) ||
       !experimentalEffectBlocksDocumentAction(true,false) ||
       !experimentalEffectBlocksDocumentAction(false,true) ||
       !experimentalEffectPlaybackReady(false,false,true,44100,512,0,0) ||
       !experimentalEffectPlaybackReady(false,true,false,44100,512,44100,512) ||
       experimentalEffectPlaybackReady(true,false,false,44100,512,44100,512) ||
       experimentalEffectPlaybackReady(false,true,true,44100,512,44100,512) ||
       experimentalEffectPlaybackReady(false,true,false,48000,512,44100,512) ||
       experimentalEffectPlaybackReady(false,true,false,44100,256,44100,512) ||
       experimentalEffectPlaybackReady(false,true,false,44100,0,44100,0) ||
       experimentalEffectPlaybackReady(false,true,false,std::numeric_limits<double>::quiet_NaN(),512,44100,512))return 1;
    std::cout<<"Experimental effect policy tests passed\n";return 0;
}
