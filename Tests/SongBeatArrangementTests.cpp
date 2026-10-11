#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-beats-"+juce::Uuid().toString());root.createDirectory();try{
    SongStudio song;song.prepare(48000);auto p=song.pattern();p.bpm=240;p.bars=3;p.rows.fill(0);p.beatGain=.2;check(song.setPattern(p),"Base");p.customBars[1]=true;p.barRows[1]={1,0,0};check(song.setPattern(p),"Bar override");check(song.undoTrackEdit()&&!song.pattern().customBars[1]&&song.redoTrackEdit()&&song.pattern().customBars[1],"Beat undo/redo");
    auto render=[](SongStudio& s){check(s.start(false),"Play");juce::AudioBuffer<float> out(2,144000);out.clear();s.process(out);s.stop();return out;};auto audio=render(song);check(audio.getMagnitude(0,0,48000)==0&&audio.getMagnitude(0,48000,48000)>.001f,"Custom bar timing");
    const auto project=root.getChildFile("beats.idwsong");check(song.save(project).wasOk(),"Save");{auto in=project.createInputStream();in->readInt();check(in->readInt()==8,"Format8");}SongStudio loaded;loaded.prepare(48000);check(loaded.load(project).wasOk()&&loaded.pattern().customBars[1]&&loaded.pattern().barRows[1][0]==1,"Load override");auto restored=render(loaded);for(int i=0;i<144000;++i)check(std::abs(audio.getSample(0,i)-restored.getSample(0,i))<.000002,"Roundtrip audio");
    const auto wav=root.getChildFile("mix.wav");check(song.exportWav(wav).wasOk(),"Mix export");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wav.createInputStream().release(),true));juce::AudioBuffer<float> exported(2,144000);check(reader&&reader->read(&exported,0,144000,0,true,true),"Read mix");for(int i=0;i<144000;++i)check(std::abs(audio.getSample(0,i)-exported.getSample(0,i))<.000002,"Export ignores custom bars");
    SongStudio::MixCheck report;check(song.checkMix(report).wasOk()&&std::abs(report.peaks[4]-audio.getMagnitude(0,144000))<.000002,"Level check ignores custom bars");check(song.exportStems(root.getChildFile("stems")).wasOk(),"Stem export");std::unique_ptr<juce::AudioFormatReader> stem(format.createReaderFor(root.getChildFile("stems/05-Drum-pattern.wav").createInputStream().release(),true));check(stem&&stem->read(&exported,0,144000,0,true,true),"Read drum stem");for(int i=0;i<144000;++i)check(std::abs(audio.getSample(0,i)-exported.getSample(0,i))<.000002,"Stem ignores custom bars");
    check(song.setSection(0,{"Hook",2,2})&&song.playSection(0,true),"Loop custom bar");juce::AudioBuffer<float> loop(2,96000);loop.clear();song.process(loop);song.stop();for(int i=0;i<48000;++i)check(std::abs(loop.getSample(0,i)-loop.getSample(0,i+48000))<.000002,"Loop uses wrong bar");
    std::uint64_t stamp=0;auto snapshot=song.recoverySnapshot(stamp);p=song.pattern();p.bars=1;song.setPattern(p);check(song.pattern().customBars[1],"Shortening lost beats");p.bars=3;p.rows={0,1,0};song.setPattern(p);check(snapshot->pattern().rows[1]==0&&snapshot->pattern().customBars[1],"Snapshot changed");check(song.pattern().rowsAtBar(1)[0]==1&&song.pattern().rowsAtBar(0)[1]==1,"Base/override independence");
    check(song.start(false)&&!song.setPattern(p),"Transport edit lock");song.stop();
    juce::MemoryBlock bytes;project.loadFileAsData(bytes);const auto bad=root.getChildFile("bad.idwsong");bad.replaceWithData(bytes.getData(),bytes.getSize()-1);check(loaded.load(bad).failed()&&loaded.pattern().customBars[1],"Truncation replaced song");auto* data=static_cast<unsigned char*>(bytes.getData());data[bytes.getSize()-4]=2;bad.replaceWithData(data,bytes.getSize());check(loaded.load(bad).failed(),"Invalid override flag accepted");
    SongStudio legacy;legacy.prepare(48000);check(legacy.save(project).wasOk()&&loaded.load(project).wasOk()&&!loaded.pattern().hasArrangement(),"Legacy load retained overrides");
    SongStudio ranges;ranges.prepare(48000);auto rangePattern=ranges.pattern();rangePattern.bars=8;check(ranges.setPattern(rangePattern),"Range setup");
    const std::array<std::uint16_t,3> hook{{0x8888,0x0101,0xAAAA}};auto rev=ranges.audioRevision();
    check(!ranges.copyBeatRange(hook,-1,3,rev)&&!ranges.copyBeatRange(hook,4,3,rev)&&!ranges.copyBeatRange(hook,0,8,rev)&&ranges.audioRevision()==rev,"Invalid range changed song");
    check(ranges.copyBeatRange(hook,4,7,rev),"Copy inclusive range");
    for(int bar=0;bar<8;++bar)check(ranges.pattern().customBars[(size_t)bar]==(bar>=4),"Range leaked into verse");
    check(ranges.pattern().rowsAtBar(7)==hook,"Last bar not copied");
    check(!ranges.copyBeatRange({0,0,0},0,0,rev),"Stale dialog overwrote song");
    check(ranges.undoTrackEdit()&&!ranges.pattern().hasArrangement(),"Range copy not one undo");
    check(ranges.redoTrackEdit()&&ranges.pattern().rowsAtBar(7)==hook,"Range redo");
    check(ranges.useBaseBeat(5)&&!ranges.pattern().customBars[5]&&ranges.pattern().customBars[4],"Use Base affected other bars");
    check(ranges.undoTrackEdit()&&ranges.pattern().customBars[5],"Use Base undo");
    check(ranges.copyBeatRange({0,0,0},0,0,ranges.audioRevision())&&ranges.pattern().customBars[0]&&ranges.pattern().rowsAtBar(0)==std::array<std::uint16_t,3>{},"Silent custom bar fell back to Base");
    check(ranges.start(false),"Range transport");rev=ranges.audioRevision();check(!ranges.copyBeatRange(hook,0,1,rev)&&!ranges.useBaseBeat(0),"Range transport lock");ranges.stop();
    std::cout<<"PASS atomic range copy, stale-dialog rejection, range undo, Base reset, silent bars, per-bar drums, base independence, bar timing, undo/redo, format8, malformed/legacy loading, recovery, shortening, loops, mix/stem and level-check parity\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
