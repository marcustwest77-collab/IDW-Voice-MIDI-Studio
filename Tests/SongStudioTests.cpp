#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
    const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-song-test-"+juce::Uuid().toString());dir.createDirectory();
    try{
        SongStudio studio;studio.prepare(48000);auto pattern=studio.pattern();pattern.bpm=240;pattern.bars=1;pattern.rows.fill(0);studio.setPattern(pattern);
        const SongStudio::Lyrics words{juce::String::fromUTF8("Café — IDW"),juce::String::fromUTF8("[Verse]\nMy words 🎵\n[Hook]\nKeep going\n")};
        check(studio.setLyrics(words),"Cannot set lyrics");
        check(studio.start(true),"Cannot start recording");
        int rendered=0;while(studio.isPlaying()){juce::AudioBuffer<float> block(1,127);for(int i=0;i<127;++i)block.setSample(0,i,.25f);studio.process(block);rendered+=127;check(rendered<50000,"Transport never ends");}
        check(studio.lyrics().text==words.text,"Recording changed lyrics");
        check(studio.hasTake(),"Recorded take missing");check(!studio.start(true),"Recording overwrote an existing take");
        const auto project=dir.getChildFile("take.idwsong");check(studio.save(project).wasOk(),"Save failed");
        check(project.getSize()==52+(juce::int64)words.title.getNumBytesAsUTF8()+(juce::int64)words.text.getNumBytesAsUTF8()+48000*8,"Recording length or project header is wrong");
        SongStudio restored;restored.prepare(44100);check(restored.load(project).wasOk(),"Project round trip failed");
        check(restored.lyrics().title==words.title&&restored.lyrics().text==words.text,"Unicode lyrics round trip failed");
        check(restored.start(false),"Playback did not start");juce::AudioBuffer<float> block(2,128);block.clear();restored.process(block);
        check(std::abs(block.getSample(0,50)-.25f)<.0001f&&std::abs(block.getSample(1,50)-.25f)<.0001f,"Mono take/stereo resampling failed");
        check(restored.setLyrics({words.title,words.text+"Live edit"}),"Lyrics edit during playback failed");
        check(restored.save(project).failed(),"Saved during active playback");restored.stop();
        const auto wav=dir.getChildFile("mix.wav");check(restored.exportWav(wav).wasOk(),"WAV export failed");juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wav.createInputStream().release(),true));
        check(reader&&reader->sampleRate==48000&&reader->bitsPerSample==24&&reader->lengthInSamples==48000&&reader->numChannels==2,"Wrong WAV format or duration");
        juce::AudioBuffer<float> exported(2,128);check(reader->read(&exported,0,128,0,true,true),"Cannot read exported mix");
        check(std::abs(exported.getSample(0,50)-.25f)<.0001f,"WAV export lost recorded audio");
        const auto bad=dir.getChildFile("bad.idwsong");juce::MemoryBlock data;project.loadFileAsData(data);bad.replaceWithData(data.getData(),data.getSize()-8);
        check(restored.load(bad).failed()&&restored.hasTake(),"Malformed project destroyed the take");
        check(restored.lyrics().text==words.text+"Live edit","Corrupt file replaced lyrics");
        juce::MemoryOutputStream legacy;
        legacy.write(data.getData(),44);legacy.setPosition(4);legacy.writeInt(1);legacy.setPosition(44);
        const int audioStart=52+(int)words.title.getNumBytesAsUTF8()+(int)words.text.getNumBytesAsUTF8();
        legacy.write(static_cast<const char*>(data.getData())+audioStart,data.getSize()-(size_t)audioStart);
        const auto v1=dir.getChildFile("legacy.idwsong");v1.replaceWithData(legacy.getData(),legacy.getDataSize());
        check(restored.load(v1).wasOk()&&restored.hasTake(),"Legacy V11 audio failed to load");
        check(restored.lyrics().title.isEmpty()&&restored.lyrics().text.isEmpty(),"Legacy load retained previous song lyrics");
        check(restored.setLyrics(words),"Restore lyrics failed");
        juce::MemoryOutputStream corrupt;
        corrupt.write(data.getData(),data.getSize());corrupt.setPosition(44);corrupt.writeInt(0x7fffffff);
        bad.replaceWithData(corrupt.getData(),corrupt.getDataSize());
        check(restored.load(bad).failed()&&restored.lyrics().text==words.text,"Oversized lyrics damaged current song");
        check(!restored.setLyrics({juce::String::repeatedString("x",257),""}),"Oversized title accepted");
        SongStudio lyricsOnly;check(lyricsOnly.setLyrics(words),"Lyrics-only set failed");
        const auto draft=dir.getChildFile("draft.idwsong");check(lyricsOnly.save(draft).wasOk(),"Lyrics-only save failed");
        check(restored.load(draft).wasOk()&&!restored.hasTake()&&restored.lyrics().text==words.text,"Lyrics-only reopen failed");
        SongStudio beats;beats.prepare(48000);auto beatPattern=beats.pattern();beatPattern.bpm=240;beatPattern.bars=1;beats.setPattern(beatPattern);check(beats.start(false),"Beat transport failed");
        juce::AudioBuffer<float> beatBlock(2,2048);beatBlock.clear();beats.process(beatBlock);check(beatBlock.getMagnitude(0,2048)>.001f,"Sequencer produced silence");beats.stop();
        restored.clearTake();check(!restored.hasTake(),"Discard did not clear take");
        std::cout<<"PASS song record boundaries / take preservation / project round trip / corrupt file rejection / sample-rate playback / WAV export / audible beat / Unicode lyrics / v1 migration / lyric limits / lyrics-only projects\n";
        dir.deleteRecursively();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';dir.deleteRecursively();return 1;}
}
