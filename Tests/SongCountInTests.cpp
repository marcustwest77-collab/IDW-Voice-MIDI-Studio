#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-count-in-"+juce::Uuid().toString());root.createDirectory();
    try{
        for(int bpm:{40,83,137,240})for(double rate:{44100.,48000.,96000.})for(int bars:{1,2}){
            const auto count=idw::countInSamples(bars,rate,bpm);
            check(idw::songBeatAt(count,rate,bpm)==bars*4&&idw::songBeatAt(count-1,rate,bpm)==bars*4-1,"Fractional count-in boundary drift");
        }
        SongStudio song;song.prepare(48000);auto p=song.pattern();p.bpm=137;p.bars=1;p.rows.fill(0);song.setPattern(p);
        check(!song.setCountInBars(3)&&song.setCountInBars(1),"Count-in validation failed");song.setMetronomeEnabled(true);
        check(song.start(true)&&song.isCountingIn(),"Count-in did not start");
        check(!song.setCountInBars(0),"Count-in changed mid-take");
        const auto pre=idw::countInSamples(1,48000,137);const auto end=p.samples(48000);
        std::int64_t time=0;bool heardClick=false;
        while(song.isPlaying()){
            juce::AudioBuffer<float> block(2,127);
            for(int i=0;i<127;++i)for(int ch=0;ch<2;++ch)block.setSample(ch,i,time+i<pre?.65f:.25f);
            song.process(block);if(time<pre&&std::abs(block.getSample(0,0)-.65f)>.001f)heardClick=true;
            time+=127;check(time<pre+end+254,"Transport never stopped");
            if(time<pre)check(!song.hasTake()&&song.seconds()==0,"Count-in audio recorded or song time advanced");
        }
        check(heardClick&&song.hasTake(),"Count-in cue/take missing");
        const auto project=root.getChildFile("take.idwsong");check(song.save(project).wasOk(),"Project save failed");
        check(project.getSize()==52+end*8,"Count-in extended recorded take");
        SongStudio restored;restored.prepare(48000);check(restored.load(project).wasOk()&&restored.start(false),"Recorded take reload failed");
        check(!restored.isCountingIn(),"Playback unexpectedly counted in");juce::AudioBuffer<float> block(2,4096);block.clear();restored.process(block);
        for(int i=0;i<4096;++i)check(std::abs(block.getSample(0,i)-.25f)<.0001f,"Count-in or metronome leaked into recorded take");restored.stop();
        const auto wav=root.getChildFile("mix.wav");check(song.exportWav(wav).wasOk(),"Export failed");juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wav.createInputStream().release(),true));
        check(reader&&reader->lengthInSamples==end,"Export contains count-in duration");
        check(reader->read(&block,0,4096,0,true,true),"Cannot read export");for(int i=0;i<4096;++i)check(std::abs(block.getSample(0,i)-.25f)<.0001f,"Metronome leaked into WAV");
        song.clearTake();check(song.start(true),"Restart failed");block.clear();song.process(block);song.stop();check(!song.hasTake()&&!song.isCountingIn(),"Cancel count-in created a take");
        check(song.start(true),"Cancelled count-in blocked next recording");song.stop();
        check(song.setCountInBars(2)&&song.start(false)&&!song.isCountingIn(),"Two-bar count-in delayed playback");block.clear();song.process(block);check(block.getMagnitude(0,4096)>.01f,"Playback metronome silent");song.stop();
        song.setMetronomeEnabled(false);song.start(false);block.clear();song.process(block);check(block.getMagnitude(0,4096)==0,"Metronome Off is audible");song.stop();
        song.setCountInBars(0);song.start(true);block.clear();song.process(block);check(song.hasTake()&&!song.isCountingIn(),"Off no longer records immediately");song.stop();
        std::cout<<"PASS count-in sample timing, monitoring, cancellation, immediate playback, click exclusion from take/WAV and legacy Off behavior\n";root.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}
}
