#pragma once
#include <JuceHeader.h>
#include <array>
class MPEAllocator {
public:
    MPEAllocator() { reset(); }
    void reset() { channels.fill(0); used.fill(false); }
    void setZone(int a, int b) { first=juce::jlimit(2,16,a); last=juce::jlimit(first,16,b); }
    int allocate(int note);
    int channelFor(int note) const;
    void release(int note);
private:
    std::array<int,128> channels{};
    std::array<bool,17> used{};
    int first=2,last=16;
};
