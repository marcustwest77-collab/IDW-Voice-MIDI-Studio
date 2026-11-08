#include "PlaybackLoop.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "ClipPlayback.h"
#include "MidiSineSynth.h"
#include "SessionPersistence.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void makeWav(const juce::File& file){
    juce::WavAudioFormat wav;auto stream=file.createOutputStream();require(stream!=nullptr,"fixture stream");
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),8000,2,24,{},0));
    require(writer!=nullptr,"fixture writer");stream.release();juce::AudioBuffer<float> source(2,800);
    for(int i=0;i<800;++i){source.setSample(0,i,0.25f);source.setSample(1,i,-0.125f);}
    const float* channels[]{source.getReadPointer(0),source.getReadPointer(1)};
    require(writer->writeFromFloatArrays(channels,2,800),"fixture samples");
}
static void wavLoop(const juce::File& file,const PlaybackLoopFrames& loop,int block,double rate=8000){
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));require(reader!=nullptr,"fixture reader");
    juce::AudioFormatReaderSource source(reader.release(),true);juce::AudioTransportSource transport;
    transport.setSource(&source,0,nullptr,8000);transport.prepareToPlay(block,rate);transport.start();
    const int total=static_cast<int>(loop.end-loop.start)*3+79;
    juce::AudioBuffer<float> scratch(2,block),output(2,total);output.clear();juce::int64 nextSource=-1;
    Clip clip{file.getFullPathName().toStdString(),0,0.5,0};std::int64_t cursor=loop.start;
    for(int pos=0;pos<total;pos+=block){const int frames=std::min(block,total-pos);
        const auto next=renderPlaybackSegments(cursor,frames,loop,[&](int offset,int count,std::int64_t frame){
            const auto window=readClipWindow(transport,scratch,nextSource,clip,300,rate,frame,count);
            require(window.offset==0 && window.frames==count,"loop seek lost clip frames");
            for(int ch=0;ch<2;++ch)output.copyFrom(ch,pos+offset,scratch,ch,0,count);
            require(applyPlaybackLoopFade(output.getWritePointer(0,pos+offset),output.getWritePointer(1,pos+offset),count,frame,loop),"WAV boundary fade");
        });require(next.has_value(),"WAV loop planning");cursor=*next;
    }
    for(int i=0;i<total;++i){const auto frame=loop.start+i%(loop.end-loop.start);const float gain=playbackLoopGain(frame,loop);
        require(std::isfinite(output.getSample(0,i)) && std::isfinite(output.getSample(1,i)) && std::abs(output.getSample(0,i))<=0.4f,"resampled loop invalid output");
        if(rate==8000 || gain==0 || (frame>=loop.start+128 && frame<loop.end-128))
            require(std::abs(output.getSample(0,i)-0.25f*gain)<0.0001f && std::abs(output.getSample(1,i)+0.125f*gain)<0.0001f,"WAV loop samples/EOF/block boundary");}
    require(cursor==loop.start+total%(loop.end-loop.start),"WAV cursor drift");
    transport.setSource(nullptr);transport.releaseResources();
}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-playback-loop","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        for(double rate:{8000,44100,48000,96000,768000})for(double bpm:{20,93,120,300}){
            const auto loop=playbackLoopFrames({1.25,5.5},bpm,rate);require(loop && loop->valid(),"valid loop compile");
            require(loop->start==std::llround(1.25*rate*60/bpm) && loop->end==std::llround(5.5*rate*60/bpm),"loop frame rounding");
        }
        const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
        for(const auto& bad:std::vector<PlaybackLoopRegion>{{-1,4},{4,4},{4,3},{0,0.249},{0,1000001},{nan,4},{0,inf},{1e308,1e308}})
            require(!playbackLoopFrames(bad,120,44100),"invalid region accepted");
        for(double bad:{nan,inf,0.0,-1.0,7999.0,768001.0})require(!playbackLoopFrames({0,4},120,bad),"invalid rate");
        for(double bad:{nan,inf,0.0,19.0,301.0})require(!playbackLoopFrames({0,4},bad,44100),"invalid tempo");
        require(playbackLoopFrames({999999.75,1000000},20,768000).has_value(),"maximum beat endpoint");
        const PlaybackLoopFrames tiny{10,14,1};
        for(int block:{1,3,4,5,17,512}){
            std::vector<std::int64_t> visited;const auto next=renderPlaybackSegments(10,block,tiny,[&](int offset,int n,std::int64_t frame){
                require(offset==static_cast<int>(visited.size()) && n>0 && frame>=10 && frame+n<=14,"segment coverage");
                for(int i=0;i<n;++i)visited.push_back(frame+i);
            });require(next && *next==10+block%4 && visited.size()==static_cast<size_t>(block),"cursor/coverage mismatch");
            for(int i=0;i<block;++i)require(visited[static_cast<size_t>(i)]==10+i%4,"skip/duplicate at wrap");
        }
        int calls=0;auto render=[&](int,int,std::int64_t){++calls;};
        require(renderPlaybackSegments(0,4,tiny,render)==10 && calls==1,"start before range");
        require(renderPlaybackSegments(19,1,tiny,render)==12,"normalize later cursor");
        calls=0;require(renderPlaybackSegments(99,0,std::nullopt,render)==99 && calls==0,"zero block");
        require(renderPlaybackSegments(99,17,std::nullopt,render)==116 && calls==1,"linear playback changed");
        calls=0;require(!renderPlaybackSegments(-1,2,tiny,render) && !renderPlaybackSegments(10,-1,tiny,render) &&
            !renderPlaybackSegments(10,65537,tiny,render) && !renderPlaybackSegments(std::numeric_limits<std::int64_t>::max(),1,std::nullopt,render) && calls==0,"invalid block invoked renderer");
        for(const auto& bad:std::vector<PlaybackLoopFrames>{{-1,4,1},{4,4,1},{4,3,1},{0,3,1},{0,100,0},{0,100,51},{0,1000,129}})
            require(!renderPlaybackSegments(0,4,bad,render) && calls==0,"invalid frames invoked renderer");
        const auto limit=std::numeric_limits<std::int64_t>::max();const PlaybackLoopFrames nearLimit{limit-8,limit,1};
        require(renderPlaybackSegments(limit-1,8,nearLimit,render)==limit-1,"frame counter overflow near limit");
        int total=0;require(renderPlaybackSegments(10,65536,tiny,[&](int,int n,std::int64_t){total+=n;})==10 && total==65536,"maximum block");
        const auto loop=*playbackLoopFrames({0.25,0.5},300,8000);
        require(loop.start==400 && loop.end==800 && loop.fadeFrames==16,"fixture loop compile");
        require(playbackLoopGain(400,loop)==0 && playbackLoopGain(799,loop)==0 && playbackLoopGain(416,loop)==1,"boundary gains");
        require(playbackLoopGain(399,loop)==0 && playbackLoopGain(800,loop)==0,"gain outside region");
        std::vector<float> dc(400,1),right(400,-1);
        require(applyPlaybackLoopFade(dc.data(),right.data(),400,400,loop),"DC fade");
        require(dc.front()==0 && dc.back()==0 && right.front()==0 && right.back()==0,"nonzero loop seam");
        for(size_t i=1;i<dc.size();++i)require(std::abs(dc[i]-dc[i-1])<=1.0f/16+1e-6f && right[i]==-dc[i],"boundary ramp discontinuity");
        float canary[2]{1,1};require(!applyPlaybackLoopFade(canary,nullptr,2,799,loop) && canary[0]==1 && canary[1]==1,"invalid fade partial mutation");
        require(applyPlaybackLoopFade(canary,canary,2,401,loop) && canary[0]==1.0f/16,"aliased mono faded twice");
        require(root.createDirectory().wasOk(),"fixture directory");const auto file=root.getChildFile("source.wav");makeWav(file);
        juce::MemoryBlock originalBytes;require(file.loadFileAsData(originalBytes),"fixture bytes");
        for(int block:{1,7,127,512,2048})wavLoop(file,loop,block);
        for(double rate:{11025,16000,44100,48000})for(int block:{1,127,512})
            wavLoop(file,*playbackLoopFrames({0.25,0.5},300,rate),block,rate);
        juce::MemoryBlock afterBytes;require(file.loadFileAsData(afterBytes) && afterBytes==originalBytes,"loop changed source WAV");
        Session session;session.bpm=300;session.addTrack(TrackType::midi);session.tracks[0].notes={{60,0.25,0.25,90}};
        const auto original=juce::JSON::toString(sessionToJson(session),true);
        for(int block:{1,7,127,512,2048}){
            juce::AudioBuffer<float> output(2,1279);output.clear();std::int64_t cursor=loop.start;
            for(int pos=0;pos<1279;pos+=block){const int n=std::min(block,1279-pos);
                const auto next=renderPlaybackSegments(cursor,n,loop,[&](int offset,int count,std::int64_t frame){
                    auto* l=output.getWritePointer(0,pos+offset);auto* r=output.getWritePointer(1,pos+offset);
                    renderMidiSine(session,l,r,count,frame,8000);require(applyPlaybackLoopFade(l,r,count,frame,loop),"MIDI fade");
                });require(next.has_value(),"MIDI loop planning");cursor=*next;
            }
            for(int i=0;i<1279;++i){const auto frame=loop.start+i%400;float l=0,r=0;renderMidiSine(session,&l,&r,1,frame,8000);
                const float gain=playbackLoopGain(frame,loop);require(std::abs(output.getSample(0,i)-l*gain)<1e-6f && std::abs(output.getSample(1,i)-r*gain)<1e-6f,"MIDI sample oracle/block independence");}
        }
        require(juce::JSON::toString(sessionToJson(session),true)==original,"loop engine changed session");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"Playback loop engine tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
