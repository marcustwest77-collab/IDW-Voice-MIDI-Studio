#include "SnapGrid.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>
using namespace idw::daw;
static bool near(double a,double b){return std::abs(a-b)<1e-10;}
int main(){
    assert(nextSnapGrid(SnapGrid::quarterBeat)==SnapGrid::sixteenthBeat);
    assert(nextSnapGrid(SnapGrid::sixteenthBeat)==SnapGrid::oneBeat);
    assert(nextSnapGrid(SnapGrid::oneBeat)==SnapGrid::off);
    assert(nextSnapGrid(SnapGrid::off)==SnapGrid::quarterBeat);
    assert(near(snapTimelineBeat(1.13,SnapGrid::quarterBeat),1.25));
    assert(near(snapTimelineBeat(1.13,SnapGrid::sixteenthBeat),1.125));
    assert(near(snapTimelineBeat(1.13,SnapGrid::oneBeat),1.0));
    assert(near(snapTimelineBeat(1.13,SnapGrid::off),1.13));
    for(auto mode:{SnapGrid::quarterBeat,SnapGrid::sixteenthBeat,SnapGrid::oneBeat,SnapGrid::off}){
        assert(near(snapTimelineBeat(-10.0,mode),0));
        assert(near(snapTimelineBeat(std::numeric_limits<double>::quiet_NaN(),mode),0));
        assert(near(snapTimelineBeat(std::numeric_limits<double>::infinity(),mode),0));
        for(double beat:{0.0,0.0625,1.0,20.0})
            assert(near(snapTimelineBeat(beat,mode),beat));
    }
    std::cout<<"Snap grid tests passed\n";
}
