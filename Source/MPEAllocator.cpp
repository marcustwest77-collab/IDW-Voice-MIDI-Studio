#include "MPEAllocator.h"
int MPEAllocator::allocate(int n) {
    if (!juce::isPositiveAndBelow(n,128)) return 0;
    if (channels[(size_t)n]) return channels[(size_t)n];
    for(int c=first;c<=last;++c) if(c!=10 && !used[(size_t)c]) {
        channels[(size_t)n]=c;used[(size_t)c]=true;return c;
    }
    return 0; // channel 10 is reserved for drums; never steal an occupied voice
}
int MPEAllocator::channelFor(int n) const { return juce::isPositiveAndBelow(n,128) ? channels[(size_t)n] : 0; }
void MPEAllocator::release(int n) {
    const int c=channelFor(n);if(c>0){used[(size_t)c]=false;channels[(size_t)n]=0;}
}
