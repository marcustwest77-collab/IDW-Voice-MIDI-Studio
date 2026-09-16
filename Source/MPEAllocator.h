#pragma once
#include <JuceHeader.h>
class MPEAllocator{public:void reset(){map.clear();used.clear();}void setZone(int a,int b){first=juce::jlimit(2,16,a);last=juce::jlimit(first,16,b);}int allocate(int);int channelFor(int)const;void release(int);
private:std::map<int,int>map;std::set<int>used;int first=2,last=16;};
