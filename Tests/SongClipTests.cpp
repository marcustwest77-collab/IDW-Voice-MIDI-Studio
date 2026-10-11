#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void fixture(const juce::File& file,float value,int rate=48000){
    juce::WavAudioFormat format;std::unique_ptr<juce::OutputStream> output=file.createOutputStream();auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(2).withBitsPerSample(24));check(writer!=nullptr,"Fixture writer");juce::AudioBuffer<float> audio(2,rate);for(int i=0;i<rate;++i){audio.setSample(0,i,value+(i%101)*.0001f);audio.setSample(1,i,value*.5f);}check(writer->writeFromAudioSampleBuffer(audio,0,rate),"Fixture write");
}
static juce::AudioBuffer<float> render(SongStudio& song){check(song.start(false),"Playback start");juce::AudioBuffer<float> audio(2,96000);audio.clear();song.process(audio);song.stop();return audio;}
static bool near(float a,float b){return std::abs(a-b)<.000002f;}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-clips-"+juce::Uuid().toString());root.createDirectory();try{
    const auto first=root.getChildFile("first.wav"),second=root.getChildFile("second.wav"),project=root.getChildFile("clips.idwsong");fixture(first,.1f);fixture(second,.2f,22050);
    SongStudio song;song.prepare(48000);auto pattern=song.pattern();pattern.bpm=240;pattern.bars=2;pattern.rows.fill(0);check(song.setPattern(pattern),"Pattern");
    check(song.addWav(0,first,0).wasOk(),"First clip import");const auto original=render(song);
    check(!song.splitClip(0,0,0)&&!song.splitClip(0,0,1)&&!song.splitClip(0,0,std::nan("")),"Invalid split accepted");
    check(song.splitClip(0,0,.375),"Split");check(song.clips(0).size()==2&&song.clips(0)[0].audio==song.clips(0)[1].audio,"Split copied source");
    const auto split=render(song);for(int i=0;i<96000;++i)check(near(original.getSample(0,i),split.getSample(0,i)),"Split changed rendered audio");
    check(song.duplicateClip(0,1,1.25),"Duplicate");auto copy=render(song);check(near(copy.getSample(0,61000),original.getSample(0,19000)),"Duplicate wrong source offset");
    auto settings=song.clips(0)[2].settings;settings.start=1;settings.gain=.5;settings.pan=1;check(song.setClipSettings(0,2,settings),"Move/mix edit");auto mixed=render(song);check(mixed.getSample(0,100)==0&&near(mixed.getSample(1,100),.025f)&&near(mixed.getSample(1,50000),.025f),"Track mix not applied to all clips");
    check(song.addWav(0,second,.5).wasOk(),"Independent source append");auto overlap=render(song);check(near(overlap.getSample(1,30000),.075f),"Overlapping sources not summed");
    const auto clipCount=song.clips(0).size();check(song.addWav(0,root.getChildFile("missing.wav"),0).failed()&&song.clips(0).size()==clipCount,"Failed import changed clips");
    check(song.duplicateClip(0,3,1.5),"Independent duplicate");check(song.save(project).wasOk(),"Format4 save");auto input=project.createInputStream();input->readInt();check(input->readInt()==4,"Wrong format");input.reset();
    check(project.getSize()<600000,"Duplicate audio serialized repeatedly");
    SongStudio restored;restored.prepare(48000);check(restored.load(project).wasOk(),"Format4 load");check(restored.clips(0).size()==5&&restored.clips(0)[3].audio==restored.clips(0)[4].audio,"Source sharing lost on reload");
    const auto expected=render(song),actual=render(restored);for(int ch=0;ch<2;++ch)for(int i=0;i<96000;++i)check(near(expected.getSample(ch,i),actual.getSample(ch,i)),"Reload changed arrangement mix");
    const auto wav=root.getChildFile("mix.wav");check(song.exportWav(wav).wasOk(),"Export");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wav.createInputStream().release(),true));juce::AudioBuffer<float> exported(2,96000);check(reader&&reader->read(&exported,0,96000,0,true,true),"Export read");for(int i=0;i<96000;++i)check(near(expected.getSample(1,i),exported.getSample(1,i)),"Export differs from playback");
    juce::MemoryBlock bytes;project.loadFileAsData(bytes);const auto bad=root.getChildFile("bad.idwsong");bad.replaceWithData(bytes.getData(),bytes.getSize()-1);check(restored.load(bad).failed()&&restored.clips(0).size()==5,"Truncated file changed project");
    // Final extra clip is a reference, followed by three empty lane counts.
    auto* data=static_cast<unsigned char*>(bytes.getData());for(int j=0;j<4;++j)data[bytes.getSize()-16+j]=0x7f;bad.replaceWithData(bytes.getData(),bytes.getSize());check(restored.load(bad).failed()&&restored.clips(0).size()==5,"Invalid source reference accepted");
    std::uint64_t stamp=0;auto snapshot=song.recoverySnapshot(stamp);check(snapshot!=nullptr,"Snapshot");
    check(song.removeClip(0,0)&&song.clips(0).size()==4,"Remove primary/promote");check(song.undoTrackEdit()&&song.clips(0).size()==5&&song.redoTrackEdit()&&song.clips(0).size()==4,"Clip undo/redo");check(snapshot->clips(0).size()==5&&snapshot->save(root.getChildFile("recovery.idwsong")).wasOk(),"Snapshot mutated");
    check(!song.start(true),"Recorded over occupied multi-clip track");check(song.armLane(2)&&song.start(true),"Empty lane recording with multi-clip backing");check(!song.splitClip(0,0,.1)&&!song.duplicateClip(0,0,0)&&!song.removeClip(0,0),"Transport edit lock");juce::AudioBuffer<float> capture(2,128);for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i)capture.setSample(ch,i,.03f);song.process(capture);song.stop();check(near(song.clips(2)[0].audio->getSample(0,20),.03f),"Backing baked into armed capture");
    SongStudio limits;limits.prepare(48000);check(limits.addWav(3,first,0).wasOk(),"Limit fixture");for(int i=1;i<64;++i)check(limits.duplicateClip(3,0,i),"Valid bounded copy rejected");check(!limits.duplicateClip(3,0,65)&&!limits.splitClip(3,0,.5)&&limits.addWav(3,second,0).failed(),"Clip limit bypassed");
    const auto bounded=root.getChildFile("bounded.idwsong");check(limits.save(bounded).wasOk(),"Bounded save");SongStudio boundedLoad;check(boundedLoad.load(bounded).wasOk()&&boundedLoad.clips(3).size()==64,"Bounded reload");
    SongStudio edge;edge.prepare(48000);check(edge.addWav(1,first,180).wasOk()&&!edge.splitClip(1,0,180.5),"Split created out-of-range clip");
    check(song.removeLane(0)&&song.clips(0).empty()&&song.undoTrackEdit()&&song.clips(0).size()==4,"Clear track history");
    SongStudio legacy;legacy.prepare(48000);legacy.setPattern(pattern);legacy.addWav(1,first,0);const auto old=root.getChildFile("old.idwsong");check(legacy.save(old).wasOk()&&restored.load(old).wasOk()&&restored.clips(0).empty()&&restored.clips(1).size()==1,"Legacy load retained new clips");
    std::cout<<"PASS multi-clip split continuity, source sharing, placement, track mixer, overlaps, independent imports, format4, malformed references, export parity, undo/recovery, armed capture, bounds and legacy migration\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
