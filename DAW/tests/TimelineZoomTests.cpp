#include "TimelineZoom.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
using namespace idw::daw;
static bool near(double a,double b){return std::abs(a-b)<1e-10;}
int main(){
    const double left=205;
    assert(near(timelinePixelsPerBeat(defaultTimelineZoom),12));
    assert(near(timelinePixelsPerBeat(-10),6));
    assert(near(timelinePixelsPerBeat(99),24));
    for(int level=0;level<=maxTimelineZoom;++level){
        for(double beat:{0.0,0.0625,1.0,4.0,16.5,100.0}){
            const auto x=timelinePixelAtBeat(beat,left,level);
            assert(near(timelineBeatAtPixel(x,left,level),beat));
        }
        assert(near(timelineBeatAtPixel(left-100,left,level),0));
        assert(near(timelinePixelAtBeat(4,left,level)-left,4*timelinePixelsPerBeat(level)));
    }
    std::cout<<"Timeline zoom mapping tests passed\n";
}
