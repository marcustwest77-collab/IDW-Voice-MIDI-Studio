#include "SessionModel.h"
#include "TimelineMath.h"
#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
using namespace idw::daw;
// Fixed-rate source simulation: verifies clip windows and summing used by realtime mixing.
static std::vector<float> render(const Session& s,const std::vector<float>& wav,int frames) {
    std::vector<float> output(frames,0);
    constexpr double rate=8.0;
    for(const auto& track:s.tracks)for(const auto& c:track.clips) {
        auto w=overlap(0,frames,rate,s.bpm,c.startBeat,c.lengthBeats,c.sourceOffsetSeconds);
        for(int i=0;i<w.frames;++i){
            const auto src=w.sourceFrame+i;
            if(src>=0 && src<(int64_t)wav.size())output[w.offset+i]+=wav[(size_t)src];
        }
    }
    return output;
}
int main(){
    Session s;s.bpm=60;assert(s.addTrack(TrackType::audio));
    std::vector<float> source(64);for(int i=0;i<64;++i)source[i]=(float)(i+1);
    assert(s.addClip(0,{"original.wav",0,4,0}));
    const auto uncut=render(s,source,32);
    assert(s.splitClip(0,0,2));
    const auto split=render(s,source,32);
    assert(split==uncut); // Sample-by-sample split continuity.
    assert(s.trimClipLeft(0,1,3));
    const auto leftTrim=render(s,source,32);
    for(int i=16;i<24;++i)assert(leftTrim[i]==0);
    assert(leftTrim[24]==source[24]); // Source offset advanced.
    assert(s.trimClipRight(0,0,1));
    const auto bothTrim=render(s,source,32);
    for(int i=8;i<24;++i)assert(bothTrim[i]==0);
    assert(bothTrim[7]==source[7] && bothTrim[24]==source[24]);
    // Overlap should sum both clips, not overwrite the first.
    Session overlapSession;overlapSession.bpm=60;
    assert(overlapSession.addTrack(TrackType::audio));
    assert(overlapSession.addClip(0,{"original.wav",0,2,0}));
    assert(overlapSession.addClip(0,{"original.wav",1,2,0}));
    const auto mixed=render(overlapSession,source,24);
    assert(mixed[7]==source[7]);
    assert(mixed[8]==source[8]+source[0]);
    assert(mixed[15]==source[15]+source[7]);
    assert(mixed[16]==source[8]);
    std::cout<<"Multi-clip timeline regression passed\\n";
}
