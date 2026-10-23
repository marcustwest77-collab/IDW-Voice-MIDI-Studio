#include "SessionModel.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
    using namespace idw::daw;
    Session s;
    assert(s.addTrack(TrackType::audio));
    assert(s.addClip(0,{"original.wav",4.0,8.0,1.0}));
    const Session before=s;
    assert(!s.splitClip(0,0,4.0));
    assert(!s.splitClip(0,0,12.0));
    assert(s.splitClip(0,0,7.0));
    assert(s.tracks[0].clips.size()==2);
    const auto& left=s.tracks[0].clips[0];
    const auto& right=s.tracks[0].clips[1];
    assert(left.sourcePath=="original.wav" && right.sourcePath=="original.wav");
    assert(std::abs(left.lengthBeats-3.0)<1e-9);
    assert(std::abs(right.startBeat-7.0)<1e-9);
    assert(std::abs(right.lengthBeats-5.0)<1e-9);
    assert(std::abs(right.sourceOffsetSeconds-2.5)<1e-9);
    assert(s.trimClipLeft(0,1,8.0));
    assert(std::abs(s.tracks[0].clips[1].sourceOffsetSeconds-3.0)<1e-9);
    assert(std::abs(s.tracks[0].clips[1].lengthBeats-4.0)<1e-9);
    assert(s.trimClipRight(0,1,10.0));
    assert(std::abs(s.tracks[0].clips[1].lengthBeats-2.0)<1e-9);
    assert(!s.trimClipRight(0,1,11.0));
    assert(!s.trimClipLeft(0,1,11.0));
    assert(s.tracks[0].clips[0].sourcePath==before.tracks[0].clips[0].sourcePath);
    std::cout<<"Clip editing regression tests passed\n";
}
