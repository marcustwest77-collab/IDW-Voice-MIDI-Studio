#include "SongStudio.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void writeFixture(const juce::File& file,int rate,float value,int channels=1){
    juce::WavAudioFormat format;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();auto writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(rate).withNumChannels(channels).withBitsPerSample(24));check(writer!=nullptr,"Fixture writer failed");juce::AudioBuffer<float> buffer(channels,rate);for(int ch=0;ch<channels;++ch)for(int i=0;i<rate;++i)buffer.setSample(ch,i,value*(ch==0?1.f:.5f));check(writer->writeFromAudioSampleBuffer(buffer,0,rate),"Fixture write failed");
}
static juce::AudioBuffer<float> render(SongStudio& studio){check(studio.start(false),"Playback failed");juce::AudioBuffer<float> audio(2,48000);audio.clear();studio.process(audio);studio.stop();return audio;}
int main(){const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("idw-arrangement-"+juce::Uuid().toString());root.createDirectory();try{
    const auto lead=root.getChildFile("lead.wav"),backing=root.getChildFile("backing.wav");writeFixture(lead,48000,.1f);writeFixture(backing,22050,.2f);
    SongStudio studio;studio.prepare(48000);auto pattern=studio.pattern();pattern.bpm=240;pattern.bars=1;pattern.rows.fill(0);studio.setPattern(pattern);
    check(studio.importWav(0,root.getChildFile("missing.wav")).failed(),"Missing WAV accepted");
    const auto stereoFile=root.getChildFile("stereo.wav");writeFixture(stereoFile,44100,.3f,2);SongStudio stereo;stereo.prepare(48000);stereo.setPattern(pattern);check(stereo.importWav(2,stereoFile).wasOk(),"Stereo import failed");auto stereoMix=render(stereo);check(std::abs(stereoMix.getSample(0,100)-.3f)<.00001f&&std::abs(stereoMix.getSample(1,100)-.15f)<.00001f,"Stereo channel identity lost");
    check(studio.importWav(0,lead).wasOk()&&studio.importWav(1,backing).wasOk(),"Mono WAV import failed");
    auto settings=studio.audioLanes()[1].settings;settings.start=.25;settings.trimIn=.1;settings.trimOut=.6;settings.gain=.5;settings.pan=1;
    check(studio.setLaneSettings(1,settings),"Valid clip edit rejected");auto audio=render(studio);
    check(std::abs(audio.getSample(0,14400)-.1f)<.00001f&&std::abs(audio.getSample(1,14400)-.2f)<.00001f,"Mix/pan/sample-rate conversion incorrect");
    check(std::abs(audio.getSample(1,4800)-.1f)<.00001f&&std::abs(audio.getSample(1,43200)-.1f)<.00001f,"Clip placement/trim boundary incorrect");
    check(studio.undoTrackEdit(),"Undo failed");check(studio.audioLanes()[1].settings.start==0,"Undo lost clip state");check(studio.redoTrackEdit()&&studio.audioLanes()[1].settings.start==.25,"Redo failed");
    const auto project=root.getChildFile("mix.idwsong");check(studio.save(project).wasOk(),"Arrangement save failed");auto input=project.createInputStream();input->readInt();check(input->readInt()==3,"Arrangement was not written as format 3");input.reset();
    SongStudio restored;restored.prepare(48000);check(restored.load(project).wasOk(),"Arrangement round trip failed");auto reopened=render(restored);check(std::abs(reopened.getSample(1,14400)-audio.getSample(1,14400))<.000001f,"Reopened mix differs");
    std::uint64_t arrangementStamp=0;auto arrangementSnapshot=studio.recoverySnapshot(arrangementStamp);check(arrangementSnapshot!=nullptr,"Arrangement recovery snapshot failed");studio.removeLane(1);
    const auto recoveredProject=root.getChildFile("recovered.idwsong");check(arrangementSnapshot->save(recoveredProject).wasOk(),"Arrangement checkpoint save failed");SongStudio recoveryCopy;recoveryCopy.prepare(48000);check(recoveryCopy.load(recoveredProject).wasOk(),"Arrangement recovery reload failed");auto recoveredMix=render(recoveryCopy);check(std::abs(recoveredMix.getSample(1,14400)-.2f)<.00001f,"Recovery lost backing clip or edits");
    const auto mix=root.getChildFile("mix.wav");check(restored.exportWav(mix).wasOk(),"Arrangement WAV export failed");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(mix.createInputStream().release(),true));juce::AudioBuffer<float> exported(2,48000);check(reader&&reader->read(&exported,0,48000,0,true,true),"Cannot read export");check(std::abs(exported.getSample(1,14400)-audio.getSample(1,14400))<.000001f,"Live mix/export mismatch");
    settings.solo=true;check(restored.setLaneSettings(1,settings),"Solo edit failed");auto solo=render(restored);check(std::abs(solo.getSample(0,14400))<.00001f&&std::abs(solo.getSample(1,14400)-.1f)<.00001f,"Solo failed");
    settings.mute=true;restored.setLaneSettings(1,settings);auto muted=render(restored);check(muted.getMagnitude(0,48000)==0,"Mute/solo precedence failed");
    settings.trimOut=2;check(!restored.setLaneSettings(1,settings),"Out-of-source trim accepted");
    juce::MemoryBlock bytes;project.loadFileAsData(bytes);const auto bad=root.getChildFile("bad.idwsong");bad.replaceWithData(bytes.getData(),bytes.getSize()-1);check(restored.load(bad).failed()&&restored.audioLanes()[1].settings.mute,"Malformed arrangement changed current state");
    check(restored.removeLane(1)&&restored.audioLanes()[1].frames==0&&restored.undoTrackEdit()&&restored.audioLanes()[1].frames==22050,"Remove/undo lost clip audio");
    SongStudio overdub;overdub.prepare(48000);overdub.setPattern(pattern);check(overdub.importWav(1,backing).wasOk()&&overdub.start(true),"Backing-assisted recording failed");juce::AudioBuffer<float> incoming(2,48000);for(int ch=0;ch<2;++ch)for(int i=0;i<48000;++i)incoming.setSample(ch,i,.1f);overdub.process(incoming);overdub.stop();check(std::abs(incoming.getSample(0,100)-.3f)<.00001f,"Backing not monitored during record");check(overdub.removeLane(1),"Backing removal failed");auto take=render(overdub);check(std::abs(take.getSample(0,100)-.1f)<.00001f,"Backing was baked into recorded take");
    std::uint64_t stamp=0;auto snapshot=overdub.recoverySnapshot(stamp);check(snapshot!=nullptr,"Recovery snapshot failed");overdub.clearTake();check(overdub.start(true),"New take start failed");incoming.clear();overdub.process(incoming);overdub.stop();auto oldTake=render(*snapshot);check(std::abs(oldTake.getSample(0,100)-.1f)<.00001f,"New recording mutated recovery/history buffer");
    // Every lane can be armed; backing is monitored but never captured.
    for(int target=0;target<4;++target){
        SongStudio armed;armed.prepare(48000);armed.setPattern(pattern);
        const int other=(target+1)%4;check(armed.importWav(other,backing).wasOk(),"Arm fixture import failed");
        check(!armed.armLane(-1)&&!armed.armLane(4)&&armed.armLane(target),"Arm bounds failed");
        check(armed.setCountInBars(1)&&armed.start(true),"Armed count-in failed");
        check(!armed.armLane(other),"Changed target during count-in");armed.stop();
        check(armed.audioLanes()[(size_t)target].frames==0,"Cancelled count-in captured audio");
        armed.setCountInBars(0);check(armed.start(true),"Armed record failed");
        juce::AudioBuffer<float> raw(target%2?1:2,48000);for(int ch=0;ch<raw.getNumChannels();++ch)for(int i=0;i<48000;++i)raw.setSample(ch,i,.1f);
        armed.process(raw);armed.stop();const auto clip=armed.audioLanes()[(size_t)target];
        check(clip.frames==48000&&std::abs(clip.audio->getSample(1,100)-.1f)<.00001f,"Armed capture/mono duplication failed");
        check(std::abs(raw.getSample(0,100)-.3f)<.00001f,"Other lanes not monitored");
        check(armed.armedLaneHasAudio()&&!armed.start(true),"Occupied armed lane overwritten");
        check(armed.undoTrackEdit()&&armed.audioLanes()[(size_t)target].frames==0&&armed.audioLanes()[(size_t)other].frames==22050,"Recording undo damaged backing");
        check(armed.redoTrackEdit()&&armed.audioLanes()[(size_t)target].frames==48000,"Recording redo failed");
        std::uint64_t revision=0;auto frozen=armed.recoverySnapshot(revision);check(frozen!=nullptr,"Armed snapshot failed");
        const auto saved=root.getChildFile("armed.idwsong");check(armed.save(saved).wasOk(),"Armed save failed");
        SongStudio loaded;loaded.armLane(3);check(loaded.load(saved).wasOk()&&loaded.armedLane()==0&&loaded.audioLanes()[(size_t)target].frames==48000,"Armed roundtrip/default arm failed");
        armed.removeLane(target);check(armed.start(true),"Armed rerecord failed");raw.clear();armed.process(raw);armed.stop();
        check(std::abs(frozen->audioLanes()[(size_t)target].audio->getSample(0,100)-.1f)<.00001f,"Rerecord mutated snapshot");
    }
    SongStudio monitor;monitor.prepare(48000);monitor.setPattern(pattern);monitor.armLane(3);
    auto monitorSettings=monitor.audioLanes()[3].settings;monitorSettings.gain=.5f;monitorSettings.pan=1;
    check(monitor.setLaneSettings(3,monitorSettings)&&monitor.start(true),"Monitor settings setup failed");
    juce::AudioBuffer<float> monitorInput(2,64);for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)monitorInput.setSample(ch,i,.2f);
    monitor.process(monitorInput);monitor.stop();
    check(monitorInput.getSample(0,10)==0&&std::abs(monitorInput.getSample(1,10)-.1f)<.00001f,"Armed pan/gain not applied to monitoring");
    check(std::abs(monitor.audioLanes()[3].audio->getSample(0,10)-.2f)<.00001f,"Monitor gain/pan baked into capture");
    std::cout<<"PASS all-lane arming, capture isolation, mono input, overwrite/transport protection, recording undo and recovery; four-track import, placement/trim, gain/pan/mute/solo, undo/redo, embedded format 3, corrupt-load preservation, mix/export parity and clean overdub capture\n";root.deleteRecursively();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';root.deleteRecursively();return 1;}}
