#include "ScaleManager.h"
int ScaleManager::quantize(int note,int root) const {
    int best=juce::jlimit(0,127,note),distance=999;
    for(int n=0;n<128;++n) {
        const int pc=(n-root+120)%12;
        if(enabled(pc) && std::abs(n-note)<distance){best=n;distance=std::abs(n-note);}
    }
    return best;
}
