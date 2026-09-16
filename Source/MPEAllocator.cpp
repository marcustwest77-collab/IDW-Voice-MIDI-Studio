#include "MPEAllocator.h"
int MPEAllocator::allocate(int n){if(map.count(n))return map[n];for(int c=first;c<=last;c++)if(!used.count(c)){map[n]=c;used.insert(c);return c;}return first;}
int MPEAllocator::channelFor(int n)const{auto i=map.find(n);return i==map.end()?1:i->second;}void MPEAllocator::release(int n){auto i=map.find(n);if(i!=map.end()){used.erase(i->second);map.erase(i);}}
