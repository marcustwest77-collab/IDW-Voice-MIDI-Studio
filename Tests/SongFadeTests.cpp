#include "SongStudio.h"
#include "SongGrid.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool near(double a,double b){return std::abs(a-b)<.000002;}
static void fixture(const juce::File& file){juce::WavAudioFormat format;std::unique_ptr<juce::OutputStream> output=file.createOutputStream();auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));check(writer!=nullptr,"Fixture writer");juce::AudioBuffer<float> audio(2,48000);for(int ch=0;ch<2;++ch)for(int i=0;i<48000;++i)audio.setSample(ch,i,.2f);check(writer->writeFromAudioSampleBuffer(audio,0,48000),"Fixture write");}
static juce::AudioBuffer<float> render(SongStudio& song){check(song.start(false),"Playback");juce::AudioBuffer<float> audio(2,96000);audio.clear();song.process(audio);song.stop();return audio;}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-fades-"+juce::Uuid().toString());root.createDirectory();try{
    check(near(idw::snapSongTime(.37,120,1),.5)&&near(idw::snapSongTime(.37,120,.5),.25)&&near(idw::snapSongTime(.37,120,.25),.375)&&near(idw::snapSongTime(3.1,120,4),4),"Grid subdivisions");
    check(near(idw::snapSongTime(.37,120,0),.37)&&near(idw::snapSongTime(.6,137,1),60./137)&&idw::snapSongTime(-1,120,1)==0&&idw::snapSongTime(200,137,1)<=180,"Grid off/fractional/bounds");
    const auto source=root.getChildFile("source.wav"),project=root.getChildFile("fades.idwsong");fixture(source);
    SongStudio song;song.prepare(48000);auto pattern=song.pattern();pattern.bpm=240;pattern.bars=2;pattern.rows.fill(0);song.setPattern(pattern);check(song.addWav(0,source,0).wasOk(),"Import");
    auto settings=song.clips(0)[0].settings;settings.fadeIn=.2;settings.fadeOut=.2;check(song.setClipSettings(0,0,settings),"Fade edit");auto faded=render(song);
    check(faded.getSample(0,0)==0&&near(faded.getSample(0,4800),.1)&&near(faded.getSample(0,24000),.2)&&near(faded.getSample(0,43200),.1),"Fade envelope samples");
    auto invalid=settings;invalid.fadeOut=.9;check(!song.setClipSettings(0,0,invalid),"Oversized fades accepted");invalid=settings;invalid.fadeIn=std::nan("");check(!song.setClipSettings(0,0,invalid),"NaN fade accepted");
    check(!song.splitClip(0,0,.1)&&!song.splitClip(0,0,.9)&&song.splitClip(0,0,.5),"Faded split boundaries");const auto split=render(song);for(int i=0;i<96000;++i)check(near(split.getSample(0,i),faded.getSample(0,i)),"Split changed outer fade audio");
    check(song.undoTrackEdit()&&song.clips(0).size()==1,"Undo faded split");
    check(song.save(project).wasOk(),"Faded single clip save");auto stream=project.createInputStream();stream->readInt();check(stream->readInt()==5,"Fades not format5");stream.reset();SongStudio restored;restored.prepare(48000);check(restored.load(project).wasOk()&&near(restored.clips(0)[0].settings.fadeOut,.2),"Faded single clip load");
    // Adjacent equal signals should stay flat through a linear crossfade.
    settings.fadeIn=0;settings.fadeOut=0;check(song.setClipSettings(0,0,settings)&&song.duplicateClip(0,0,.75)&&song.crossfadeNext(0,0),"Crossfade setup");
    check(near(song.clips(0)[0].settings.fadeOut,.25)&&near(song.clips(0)[1].settings.fadeIn,.25),"Crossfade overlap length");
    const auto cross=render(song);for(int i=36000;i<47999;++i)check(near(cross.getSample(0,i),.2),"Crossfade gain bump/drop");
    check(!song.crossfadeNext(0,1),"Missing next clip accepted");check(song.undoTrackEdit()&&song.clips(0)[0].settings.fadeOut==0&&song.redoTrackEdit(),"Crossfade undo/redo");
    check(song.save(project).wasOk()&&restored.load(project).wasOk(),"Multi-clip fade roundtrip");const auto loaded=render(restored);for(int i=0;i<96000;++i)check(near(cross.getSample(0,i),loaded.getSample(0,i)),"Fade persistence changed playback");
    const auto wav=root.getChildFile("mix.wav");check(song.exportWav(wav).wasOk(),"Fade export");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wav.createInputStream().release(),true));juce::AudioBuffer<float> exported(2,96000);check(reader&&reader->read(&exported,0,96000,0,true,true),"Export read");for(int i=0;i<96000;++i)check(near(cross.getSample(0,i),exported.getSample(0,i)),"Export fade mismatch");
    std::uint64_t stamp=0;auto snapshot=song.recoverySnapshot(stamp);check(snapshot!=nullptr,"Snapshot");settings=song.clips(0)[0].settings;settings.fadeOut=0;song.setClipSettings(0,0,settings);check(near(snapshot->clips(0)[0].settings.fadeOut,.25),"Snapshot fade mutated");
    juce::MemoryBlock data;project.loadFileAsData(data);const auto bad=root.getChildFile("bad.idwsong");bad.replaceWithData(data.getData(),data.getSize()-1);check(restored.load(bad).failed()&&restored.clips(0).size()==2,"Truncated fades changed song");
    // Corrupt the final empty track's fade-out with a finite nonzero duration.
    juce::MemoryOutputStream bytes;bytes.write(data.getData(),data.getSize()-8);bytes.writeDouble(100);bad.replaceWithData(bytes.getData(),bytes.getDataSize());check(restored.load(bad).failed()&&restored.clips(0).size()==2,"Invalid fade metadata accepted");
    auto move=song.clips(0)[1].settings;move.start=1.25;song.setClipSettings(0,1,move);check(!song.crossfadeNext(0,0),"Gap crossfade accepted");move.start=.1;move.trimOut=.4;move.fadeIn=0;song.setClipSettings(0,1,move);check(!song.crossfadeNext(0,0),"Contained clip crossfade accepted");
    check(song.start(false)&&!song.crossfadeNext(0,0),"Transport crossfade edit allowed");song.stop();
    std::cout<<"PASS fades, split continuity, crossfade gain, undo, format5, corrupt-load preservation, export/recovery and musical grid\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
