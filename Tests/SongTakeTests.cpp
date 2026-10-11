#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static bool near(float a,float b){return std::abs(a-b)<.000002f;}
static juce::AudioBuffer<float> render(SongStudio& song){check(song.start(false),"Start");juce::AudioBuffer<float> buffer(2,96000);buffer.clear();song.process(buffer);song.stop();return buffer;}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-takes-"+juce::Uuid().toString());root.createDirectory();try{
    const auto wav=root.getChildFile("source.wav"),project=root.getChildFile("takes.idwsong"),bad=root.getChildFile("bad.idwsong");
    {juce::WavAudioFormat format;std::unique_ptr<juce::OutputStream> output=wav.createOutputStream();auto writer=format.createWriterFor(output,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));check(writer!=nullptr,"Writer");juce::AudioBuffer<float> audio(2,48000);for(int ch=0;ch<2;++ch)for(int i=0;i<48000;++i)audio.setSample(ch,i,.1f);check(writer->writeFromAudioSampleBuffer(audio,0,48000),"Fixture");}
    for(int lane=0;lane<4;++lane){
        SongStudio song;song.prepare(48000);auto pattern=song.pattern();pattern.bpm=240;pattern.bars=4;pattern.rows.fill(0);pattern.trackGain=1;check(song.setPattern(pattern),"Pattern");
        check(song.addWav(lane,wav,0).wasOk()&&song.duplicateClip(lane,0,.5)&&song.duplicateClip(lane,0,1.5)&&song.addWav((lane+1)%4,wav,0).wasOk(),"Clips");
        const auto source=song.clips(lane)[0].audio;check(song.selectTake(lane,1),"Select take");auto clips=song.clips(lane);
        check(clips[0].settings.clipMuted&&!clips[1].settings.clipMuted&&!clips[2].settings.clipMuted,"Direct overlap / touching endpoint");check(!song.clips((lane+1)%4)[0].settings.clipMuted,"Other track touched");
        auto audio=render(song);check(near(audio.getSample(0,100),.1f)&&near(audio.getSample(0,30000),.2f),"Primary mute playback");
        check(song.undoTrackEdit()&&!song.clips(lane)[0].settings.clipMuted&&song.redoTrackEdit()&&song.clips(lane)[0].settings.clipMuted,"Take undo/redo");
        check(song.setClipMuted(lane,2,true)&&song.selectTake(lane,0),"Select primary");check(!song.clips(lane)[0].settings.clipMuted&&song.clips(lane)[1].settings.clipMuted&&song.clips(lane)[2].settings.clipMuted,"Disjoint mute preserved");
        audio=render(song);check(near(audio.getSample(0,30000),.2f)&&near(audio.getSample(0,60000),0),"Extra mute playback");
        auto mix=song.clips(lane)[1].settings;mix.gain=.5f;mix.pan=1;mix.solo=true;check(song.setClipSettings(lane,1,mix),"Mixer");audio=render(song);check(near(audio.getSample(0,100),0)&&near(audio.getSample(1,100),.05f),"Solo/mixer separate from clip mute");
        check(song.save(project).wasOk(),"Save");{auto input=project.createInputStream();input->readInt();check(input->readInt()==7,"Version 7 required");}
        SongStudio loaded;loaded.prepare(48000);check(loaded.load(project).wasOk(),"Load");check(loaded.clips(lane)[1].settings.clipMuted&&loaded.clips(lane)[2].settings.clipMuted&&!loaded.clips(lane)[0].settings.clipMuted,"Mute roundtrip");auto restored=render(loaded);for(int ch=0;ch<2;++ch)for(int i=0;i<96000;++i)check(near(audio.getSample(ch,i),restored.getSample(ch,i)),"Roundtrip audio");
        const auto exported=root.getChildFile("export.wav");check(loaded.exportWav(exported).wasOk(),"Export");{juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(exported.createInputStream().release(),true));juce::AudioBuffer<float> output(2,96000);check(reader&&reader->read(&output,0,96000,0,true,true),"Read export");for(int ch=0;ch<2;++ch)for(int i=0;i<96000;++i)check(near(output.getSample(ch,i),audio.getSample(ch,i)),"Export mute parity");}
        juce::MemoryBlock bytes;project.loadFileAsData(bytes);bad.replaceWithData(bytes.getData(),bytes.getSize()-1);check(loaded.load(bad).failed()&&loaded.clips(lane)[1].settings.clipMuted,"Truncated load changed current project");static_cast<unsigned char*>(bytes.getData())[bytes.getSize()-4]=2;bad.replaceWithData(bytes.getData(),bytes.getSize());check(loaded.load(bad).failed(),"Invalid flag accepted");
        std::uint64_t stamp=0;auto snapshot=song.recoverySnapshot(stamp);check(snapshot!=nullptr,"Recovery snapshot");check(song.selectTake(lane,0,true)&&!song.clips(lane)[1].settings.clipMuted&&song.clips(lane)[2].settings.clipMuted,"Unmute overlap respects disjoint clips");check(snapshot->clips(lane)[1].settings.clipMuted,"Recovery mutated");
        check(song.setClipMuted(lane,1,true)&&song.removeClip(lane,0)&&song.clips(lane)[0].settings.clipMuted,"Promotion lost clip mute");check(song.duplicateClip(lane,0,3)&&song.clips(lane).back().settings.clipMuted&&song.splitClip(lane,0,1)&&song.clips(lane).back().settings.clipMuted,"Copy/split lost mute");check(source->getSample(0,100)>.09f,"Source changed");
        check(song.start(false)&&!song.setClipMuted(lane,0,false)&&!song.selectTake(lane,0),"Playback edit lock");song.stop();check(!song.selectTake(-1,0)&&!song.setClipMuted(lane,99,true),"Invalid indices");
        check(song.armLane(lane)&&song.recordAtCursor(0),"Record new take");juce::AudioBuffer<float> input(2,100);input.clear();song.process(input);song.stop();check(!song.clips(lane).back().settings.clipMuted,"New recording inherited mute");
        SongStudio legacy;legacy.prepare(48000);check(legacy.addWav(lane,wav,0).wasOk()&&legacy.save(project).wasOk()&&loaded.load(project).wasOk()&&!loaded.clips(lane)[0].settings.clipMuted,"Legacy default mute");
    }
    // Overlap is based on trimmed intervals and never expands transitively.
    SongStudio intervals;intervals.prepare(48000);check(intervals.addWav(0,wav,0).wasOk()&&intervals.duplicateClip(0,0,.75)&&intervals.duplicateClip(0,0,1.5),"Overlap chain");
    check(intervals.selectTake(0,0)&&intervals.clips(0)[1].settings.clipMuted&&!intervals.clips(0)[2].settings.clipMuted,"Selection expanded transitively");
    check(intervals.selectTake(0,0,true),"Unmute chain");auto trimmed=intervals.clips(0)[0].settings;trimmed.trimIn=.25;trimmed.trimOut=.75;check(intervals.setClipSettings(0,0,trimmed)&&intervals.selectTake(0,0)&&!intervals.clips(0)[1].settings.clipMuted,"Overlap ignored trims");
    check(intervals.save(project).wasOk(),"Unmuted save");{auto stream=project.createInputStream();stream->readInt();check(stream->readInt()==4,"All-unmuted project unnecessarily requires format7");}
    std::cout<<"PASS take selection on all tracks, whole-clip mute, boundaries, mixer, source preservation, undo/recovery, format7 validation, export parity and new recordings\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
