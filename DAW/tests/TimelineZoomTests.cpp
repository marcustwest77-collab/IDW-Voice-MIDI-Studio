#include "TimelineZoom.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
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
    // Scroll is a visual viewport offset, never a modification to clip beats.
    for(int level=0;level<=maxTimelineZoom;++level){
        for(double scroll:{0.0,4.0,12.5,128.0}){
            for(double beat:{scroll,scroll+0.0625,scroll+4.0,scroll+40.0}){
                const auto x=timelinePixelAtBeat(beat,left,level,scroll);
                assert(near(timelineBeatAtPixel(x,left,level,scroll),beat));
            }
            assert(near(timelinePixelAtBeat(scroll,left,level,scroll),left));
        }
        const double viewport=600.0;
        assert(near(timelineMaxScroll(0.0,viewport,level),0.0));
        assert(near(timelineMaxScroll(4.0,10000.0,level),0.0));
        assert(near(timelineClampScroll(-5.0,200,viewport,level),0.0));
        assert(near(timelineClampScroll(1e9,200,viewport,level),
                    timelineMaxScroll(200,viewport,level)));
        assert(near(timelineClampScroll(std::numeric_limits<double>::infinity(),
                                       200,viewport,level),0.0));
    }
    std::cout<<"Timeline zoom mapping tests passed\n";
}
