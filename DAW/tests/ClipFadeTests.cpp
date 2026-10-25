#include "ClipEnvelope.h"
#include "SessionHistory.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <limits>
#include <iostream>
using namespace idw::daw;
static bool near(double a,double b){return std::abs(a-b)<1e-6;}
int main(){
    Session s;s.bpm=60;assert(s.addTrack(TrackType::audio));
    assert(s.addClip(0,{"source.wav",1,4,0.25}));
    assert(s.setClipFades(0,0,1,1));
    ClipEnvelope e(s.tracks[0].clips[0],60,8);
    assert(e.gain(7)==0 && e.gain(8)==0 && e.gain(39)==0 && e.gain(40)==0);
    assert(near(e.gain(11),3.0/7) && e.gain(16)==1 && near(e.gain(36),3.0/7));
    std::vector<float> whole(48,1),blocks(48,1);
    e.apply(whole.data(),48,0);
    for(int start=0;start<48;start+=3)e.apply(blocks.data()+start,3,start);
    assert(whole==blocks); // Same production function as realtime scratch buffers.
    assert(!s.setClipFades(0,0,-1,0));
    assert(!s.setClipFades(0,0,5,0));
    assert(!s.setClipFades(0,0,std::numeric_limits<double>::quiet_NaN(),0));
    assert(!s.setClipFades(0,0,0,0,(FadeCurve)9));
    assert(s.tracks[0].clips[0].fadeInBeats==1);
    assert(!s.splitClip(0,0,1.5)); // Refuse to alter a running envelope.
    const auto unsplit=s;
    assert(s.splitClip(0,0,3));
    for(int frame=0;frame<48;++frame)
        assert(near(e.gain(frame),ClipEnvelope(s.tracks[0].clips[0],60,8).gain(frame)+
                    ClipEnvelope(s.tracks[0].clips[1],60,8).gain(frame)));
    assert(s.resizeClip(0,0,0.5) && s.tracks[0].clips[0].fadeInBeats==0.5);
    s=unsplit;assert(s.trimClipLeft(0,0,4.5));
    assert(s.tracks[0].clips[0].fadeInBeats==0.5 && s.tracks[0].clips[0].sourceOffsetSeconds==3.75);
    Session pair;pair.bpm=60;assert(pair.addTrack(TrackType::audio));
    assert(pair.addClip(0,{"a.wav",0,3,0.2}));
    assert(pair.addClip(0,{"b.wav",2,3,0.3}));
    ClipEditHistory history;const auto beforeCrossfade=pair;
    assert(pair.crossfadeClips(0,1,0,FadeCurve::linear));
    for(int frame=16;frame<24;++frame)
        assert(near(ClipEnvelope(pair.tracks[0].clips[0],60,8).gain(frame)+
                    ClipEnvelope(pair.tracks[0].clips[1],60,8).gain(frame),1));
    assert(history.record(beforeCrossfade,pair));
    assert(history.undo(pair)==ClipEditHistory::Result::applied);
    assert(pair.tracks[0].clips[0].fadeOutBeats==0 && pair.tracks[0].clips[1].fadeInBeats==0);
    assert(history.redo(pair)==ClipEditHistory::Result::applied);
    assert(pair.tracks[0].clips[0].fadeOutBeats==1 && pair.tracks[0].clips[1].fadeInBeats==1);
    assert(pair.tracks[0].clips[0].sourcePath=="a.wav" && pair.tracks[0].clips[0].sourceOffsetSeconds==0.2);
    assert(pair.crossfadeClips(0,0,1,FadeCurve::equalPower));
    for(int frame=16;frame<24;++frame){
        const auto a=ClipEnvelope(pair.tracks[0].clips[0],60,8).gain(frame);
        const auto b=ClipEnvelope(pair.tracks[0].clips[1],60,8).gain(frame);
        assert(near(a*a+b*b,1));
    }
    assert(!pair.crossfadeClips(0,0,0,FadeCurve::linear));
    assert(pair.addClip(0,{"third.wav",2.5,1,0}));
    assert(!pair.crossfadeClips(0,0,1,FadeCurve::linear));
    pair.tracks[0].clips.pop_back();
    assert(pair.moveClip(0,1,3));assert(!pair.crossfadeClips(0,0,1,FadeCurve::linear));
    assert(pair.moveClip(0,1,0));assert(!pair.crossfadeClips(0,0,1,FadeCurve::linear));
    // Fractional beats, all supported output rates, and one-sample overlaps.
    for(double rate:{8000.0,44100.0,48000.0,96000.0}){
        Session fractional;fractional.bpm=137;fractional.addTrack(TrackType::audio);
        assert(fractional.addClip(0,{"a",0.123,1.789,0}));
        assert(fractional.addClip(0,{"b",1.111,2.0,0}));
        assert(fractional.crossfadeClips(0,0,1,FadeCurve::linear));
        const ClipEnvelope a(fractional.tracks[0].clips[0],137,rate),b(fractional.tracks[0].clips[1],137,rate);
        for(auto f=b.start;f<a.start+a.length;++f)assert(near(a.gain(f)+b.gain(f),1));
    }
    Clip a{"a",0,2,0,0,1},b{"b",1,2,0,1,0};
    assert(ClipEnvelope(a,60,1).gain(1)+ClipEnvelope(b,60,1).gain(1)==1);
    // At half-sample positions, unfaded windows and envelopes must agree on
    // every audible sample. The former reordered envelope math lost the entry
    // sample at e.g. 7.5 * 137 / (60 * 44100) beats.
    for(double rate:{8000.0,44100.0,48000.0,96000.0}){
        for(int n=0;n<128;++n){
            Clip clip{"clock.wav",(n+0.5)*137.0/(60.0*rate),20.0*137.0/(60.0*rate),0};
            const auto window=overlap(0,160,rate,137,clip.startBeat,clip.lengthBeats);
            const ClipEnvelope envelope(clip,137,rate);
            assert(envelope.valid && envelope.start==window.offset && envelope.length==window.frames);
            for(int frame=0;frame<160;++frame)
                assert(envelope.gain(frame)==(frame>=window.offset && frame<window.offset+window.frames?1.0f:0.0f));
        }
    }
    for(double invalid:{-1.0,std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity(),1.0e300}){
        Clip invalidClip{"bad",invalid,1,0};
        const ClipEnvelope invalidEnvelope(invalidClip,120,48000);
        assert(!invalidEnvelope.valid && invalidEnvelope.gain(0)==0);
        invalidClip={"bad",0,1,0};invalidClip.fadeInBeats=invalid;
        assert(!ClipEnvelope(invalidClip,120,48000).valid);
    }
    assert(!ClipEnvelope(a,0,48000).valid);
    assert(!ClipEnvelope(a,120,std::numeric_limits<double>::infinity()).valid);
    assert(e.gain(std::numeric_limits<int64_t>::min())==0);
    // Clip gain is independent of track gain and survives edit/history operations.
    Session gainSession;gainSession.addTrack(TrackType::audio);
    assert(gainSession.addClip(0,{"gain.wav",0,4,0}));
    assert(gainSession.addClip(0,{"other.wav",5,4,0}));
    const auto originalGainSession=gainSession;
    ClipEditHistory gainHistory;
    assert(gainSession.setClipGain(0,0,-6.0));
    assert(gainHistory.record(originalGainSession,gainSession));
    assert(gainSession.tracks[0].gain==1 && gainSession.tracks[0].clips[1].gainDb==0);
    assert(near(ClipEnvelope(gainSession.tracks[0].clips[0],120,48000).gain(100),
                std::pow(10.0,-6.0/20.0)));
    assert(gainHistory.undo(gainSession)==ClipEditHistory::Result::applied);
    assert(gainSession.tracks[0].clips[0].gainDb==0);
    assert(gainHistory.redo(gainSession)==ClipEditHistory::Result::applied);
    assert(gainSession.tracks[0].clips[0].gainDb==-6);
    assert(gainSession.splitClip(0,0,2));
    assert(gainSession.tracks[0].clips[0].gainDb==-6 && gainSession.tracks[0].clips[1].gainDb==-6);
    assert(gainSession.trimClipLeft(0,0,0.5) && gainSession.resizeClip(0,0,1));
    assert(gainSession.tracks[0].clips[0].gainDb==-6);
    for(double invalid:{-60.1,12.1,std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()}){
        assert(!gainSession.setClipGain(0,0,invalid) && gainSession.tracks[0].clips[0].gainDb==-6);
        auto clip=gainSession.tracks[0].clips[0];clip.gainDb=invalid;
        assert(!gainSession.addClip(0,clip));
        assert(!ClipEnvelope(clip,120,48000).valid);
    }
    assert(gainSession.setClipGain(0,0,-60) && gainSession.setClipGain(0,0,12));
    std::cout<<"Clip fade model, history and playback envelope regressions passed\n";
}

