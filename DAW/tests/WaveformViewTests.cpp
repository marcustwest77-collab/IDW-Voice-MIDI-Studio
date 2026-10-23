#include "WaveformView.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>
using namespace idw::daw;
static bool near(double a,double b){return std::abs(a-b)<1e-9;}
int main(){
    Session s;s.bpm=120;assert(s.addTrack(TrackType::audio));
    assert(s.addClip(0,{"voice.wav",4,8,1.25}));
    auto range=clipWaveformWindow(s.tracks[0].clips[0],s.bpm);
    assert(range && near(range->startSeconds,1.25) && near(range->endSeconds,5.25));
    // Moving a clip changes the timeline location, never the source waveform.
    assert(s.moveClip(0,0,12));
    auto moved=clipWaveformWindow(s.tracks[0].clips[0],s.bpm);
    assert(moved && near(moved->startSeconds,1.25) && near(moved->endSeconds,5.25));
    assert(s.splitClip(0,0,16));
    auto first=clipWaveformWindow(s.tracks[0].clips[0],s.bpm);
    auto second=clipWaveformWindow(s.tracks[0].clips[1],s.bpm);
    assert(first && second && near(first->endSeconds,second->startSeconds));
    assert(near(second->endSeconds,5.25));
    assert(s.trimClipLeft(0,1,17));
    auto trimmed=clipWaveformWindow(s.tracks[0].clips[1],s.bpm);
    assert(trimmed && near(trimmed->startSeconds,3.75) && near(trimmed->endSeconds,5.25));
    // A longer clip may extend past the source's EOF; do not stretch its image.
    Clip beyond{"short.wav",0,8,9};
    auto extended=clipWaveformWindow(beyond,120);
    assert(extended && near(extended->startSeconds,9) && near(extended->endSeconds,13));
    for(double bad:{0.0,-1.0,std::numeric_limits<double>::quiet_NaN(),
                    std::numeric_limits<double>::infinity()}){
        assert(!clipWaveformWindow(beyond,bad));
        Clip invalid=beyond;invalid.sourceOffsetSeconds=bad;
        assert(!clipWaveformWindow(invalid,120));
        invalid=beyond;invalid.lengthBeats=bad;
        assert(!clipWaveformWindow(invalid,120));
    }
    Clip huge=beyond;huge.sourceOffsetSeconds=1e308;huge.lengthBeats=1e308;
    assert(!clipWaveformWindow(huge,120));
    std::cout<<"Waveform source-window mapping tests passed\n";
}
