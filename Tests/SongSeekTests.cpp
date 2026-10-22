#include "SongStudio.h"
#include "SongTimelineView.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool near(double a,double b){return std::abs(a-b)<.000002;}
int main(){try{
    idw::SongTimelineView view;view.setDuration(16);view.setZoom(4,10);check(near(view.span(),4)&&near(view.start,8)&&near(view.timeAt(.5),10)&&near(view.fractionAt(10),.5),"Zoom coordinate transform");
    view.setStart(20);check(near(view.start,12)&&near(view.timeAt(1),16)&&!view.contains(11),"Scroll/end bounds");view.setZoom(32,0);check(view.start==0&&near(view.span(),.5),"Cursor anchor at start");view.setDuration(2);check(view.start==0&&near(view.span(),.0625),"Duration change");view.setStart(1.9);check(near(view.timeAt(.5),1.93125),"Scrolled hit test time");view.fit();check(view.start==0&&view.zoom==1&&near(view.timeAt(.5),1),"Fit song");view.setZoom(std::nan(""),0);check(view.zoom==1,"NaN zoom changed view");
    SongStudio song;song.prepare(48000);auto pattern=song.pattern();pattern.bpm=240;pattern.bars=3;pattern.rows.fill(0);song.setPattern(pattern);check(!song.seekPlayback(1)&&song.start(true),"Stopped seek/start");juce::AudioBuffer<float> source(2,144000);for(int i=0;i<144000;++i)for(int ch=0;ch<2;++ch)source.setSample(ch,i,.1f+(i%1700)*.00005f);check(!song.seekPlayback(1),"Seek allowed while recording");song.process(source);song.stop();const auto revision=song.audioRevision();
    check(song.start(false,1.5)&&near(song.seconds(),1.5),"Play cursor start");juce::AudioBuffer<float> block(2,127);block.clear();song.process(block);for(int i=0;i<127;++i)check(near(block.getSample(0,i),source.getSample(0,72000+i)),"Play cursor wrong source");
    check(song.seekPlayback(.25)&&near(song.seconds(),.25),"Seek backwards before initial play cursor");block.clear();song.process(block);for(int i=0;i<127;++i)check(near(block.getSample(0,i),source.getSample(0,12000+i)),"Backward seek wrong source");
    const auto at=song.seconds();check(!song.seekPlayback(3)&&!song.seekPlayback(-1)&&!song.seekPlayback(std::nan(""))&&near(song.seconds(),at),"Invalid seek changed transport");
    check(song.seekPlayback(2.999),"Seek near end");juce::AudioBuffer<float> tail(2,128);tail.clear();song.process(tail);check(!song.isPlaying()&&tail.getSample(0,48)==0,"End stop after seek");check(song.audioRevision()==revision,"Seek changed project revision");
    check(song.setSection(0,{"Hook",2,2})&&song.playSection(0,true),"Loop seek setup");check(!song.seekPlayback(.9)&&!song.seekPlayback(2)&&song.seekPlayback(1.999),"Loop range seek bounds");tail.clear();song.process(tail);check(song.isLooping()&&near(tail.getSample(0,48),source.getSample(0,48000)),"Seek lost loop wrap");song.stop();
    check(song.playSection(0,false)&&!song.seekPlayback(.5)&&song.seekPlayback(1.5),"One-shot seek bounds");song.stop();check(!song.start(false,3),"Play cursor accepted song end");
    song.armLane(2);song.setCountInBars(1);check(song.start(true)&&song.isCountingIn()&&!song.seekPlayback(1),"Count-in seek protection");song.stop();
    // Seeking restarts built-in rhythm from the same deterministic state.
    SongStudio rhythm;rhythm.prepare(48000);rhythm.setMetronomeEnabled(true);check(rhythm.start(false)&&rhythm.seekPlayback(1.2),"Rhythm seek");juce::AudioBuffer<float> a(2,512),b(2,512);a.clear();b.clear();rhythm.process(a);check(rhythm.seekPlayback(1.2),"Repeat rhythm seek");rhythm.process(b);for(int i=0;i<512;++i)check(near(a.getSample(0,i),b.getSample(0,i)),"Rhythm seek reset mismatch");rhythm.stop();
    std::cout<<"PASS zoom/scroll coordinates, bounds, fit, cursor playback, backward seek, invalid seek, end stop, loop/one-shot bounds, recording/count-in protection and rhythm reset\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
