#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-song-test-"+juce::Uuid().toString());dir.createDirectory();
    try{
        SongStudio studio;studio.prepare(48000);auto pattern=studio.pattern();pattern.bpm=240;pattern.bars=1;pattern.rows.fill(0);studio.setPattern(pattern);
        check(studio.start(true),"Cannot start recording");
        int rendered=0;while(studio.isPlaying()){juce::AudioBuffer<float> block(1,127);for(int i=0;i<127;++i)block.setSample(0,i,.25f);studio.process(block);rendered+=127;check(rendered<50000,"Transport never ends");}
        check(studio.hasTake(),"Recorded take missing");check(!studio.start(true),"Recording overwrote an existing take");
        const auto project=dir.getChildFile("take.idwsong");check(studio.save(project).wasOk(),"Save failed");
        check(project.getSize()==44+48000*8,"Recording length or project header is wrong");
        SongStudio restored;restored.prepare(44100);check(restored.load(project).wasOk(),"Project round trip failed");
        check(restored.start(false),"Playback did not start");juce::AudioBuffer<float> block(2,128);block.clear();restored.process(block);
        check(std::abs(block.getSample(0,50)-.25f)<.0001f&&std::abs(block.getSample(1,50)-.25f)<.0001f,"Mono take/stereo resampling failed");
        check(restored.save(project).failed(),"Saved during active playback");restored.stop();
        const auto wav=dir.getChildFile("mix.wav");check(restored.exportWav(wav).wasOk(),"WAV export failed");juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wav.createInputStream().release(),true));
        check(reader&&reader->sampleRate==48000&&reader->bitsPerSample==24&&reader->lengthInSamples==48000&&reader->numChannels==2,"Wrong WAV format or duration");
        juce::AudioBuffer<float> exported(2,128);check(reader->read(&exported,0,128,0,true,true),"Cannot read exported mix");
        check(std::abs(exported.getSample(0,50)-.25f)<.0001f,"WAV export lost recorded audio");
        const auto bad=dir.getChildFile("bad.idwsong");juce::MemoryBlock data;project.loadFileAsData(data);bad.replaceWithData(data.getData(),data.getSize()-8);
        check(restored.load(bad).failed()&&restored.hasTake(),"Malformed project destroyed the take");
        SongStudio beats;beats.prepare(48000);auto beatPattern=beats.pattern();beatPattern.bpm=240;beatPattern.bars=1;beats.setPattern(beatPattern);check(beats.start(false),"Beat transport failed");
        juce::AudioBuffer<float> beatBlock(2,2048);beatBlock.clear();beats.process(beatBlock);check(beatBlock.getMagnitude(0,2048)>.001f,"Sequencer produced silence");beats.stop();
        restored.clearTake();check(!restored.hasTake(),"Discard did not clear take");
        std::cout<<"PASS song record boundaries / take preservation / project round trip / corrupt file rejection / sample-rate playback / WAV export / audible beat\n";
        dir.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';dir.deleteRecursively();return 1;}
}
