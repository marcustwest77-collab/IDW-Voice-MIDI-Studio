#include "AudioRelink.h"
#include "SessionHistory.h"
#include "SessionPersistence.h"
#include "SessionRecovery.h"
#include "ClipPlayback.h"
#include "OfflineBounce.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static juce::String json(const Session& session){return juce::JSON::toString(sessionToJson(session),true);}
static void makeWav(const juce::File& file,int frames){
    juce::WavAudioFormat wav;
    auto stream=std::unique_ptr<juce::FileOutputStream>(file.createOutputStream());
    require(stream!=nullptr,"fixture stream");
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(),44100,2,24,{},0));
    require(writer!=nullptr,"fixture writer");stream.release();
    juce::AudioBuffer<float> data(2,frames);
    for(int i=0;i<frames;++i){data.setSample(0,i,0.1f+i/8192.0f);data.setSample(1,i,-0.05f-i/16384.0f);}
    const float* channels[]{data.getReadPointer(0),data.getReadPointer(1)};
    require(writer->writeFromFloatArrays(channels,2,frames),"fixture write");
}
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-relink","",false);
    require(root.createDirectory().wasOk(),"fixture directory");
    const auto old=root.getChildFile("original.wav"),moved=root.getChildFile("moved.wav");
    makeWav(old,256);juce::MemoryBlock originalBytes;require(old.loadFileAsData(originalBytes),"source bytes");
    Session session;session.bpm=60;session.addTrack(TrackType::audio);
    const auto oldPath=old.getFullPathName().toStdString();
    session.addClip(0,{oldPath,0,128.0/44100,16.0/44100});
    session.addClip(0,{oldPath,64.0/44100,128.0/44100,32.0/44100});
    session.crossfadeClips(0,0,1,FadeCurve::equalPower);session.setClipGain(0,0,-6);
    session.addTrack(TrackType::audio);session.addClip(1,{oldPath,0,64.0/44100,0});
    session.tracks[1].mute=true;session.tracks[0].name="Vocals";
    // A different missing source with the same basename must not be remapped.
    const auto other=root.getChildFile("elsewhere/original.wav").getFullPathName().toStdString();
    session.addTrack(TrackType::audio);session.addClip(2,{other,0,16.0/44100,0});session.tracks[2].mute=true;
    require(old.moveFileTo(moved),"move original fixture");
    const auto before=json(session);
    require(missingAudioSources(session,formats).size()==2,"unique missing sources");
    const auto shortFile=root.getChildFile("short.wav");makeWav(shortFile,64);
    const auto invalid=root.getChildFile("invalid.wav");require(invalid.replaceWithText("not audio"),"invalid fixture");
    Session candidate=session;juce::String error;
    for(const auto& file:{shortFile,invalid,root.getChildFile("absent.wav")}){
        require(!prepareAudioRelink(session,oldPath,file,formats,candidate,error),"bad replacement accepted");
        require(json(session)==before && json(candidate)==before && error.isNotEmpty(),"failure mutated project");
    }
    require(prepareAudioRelink(session,oldPath,moved,formats,candidate,error),"valid relink");
    auto expected=session;
    for(auto& track:expected.tracks)for(auto& clip:track.clips)
        if(clip.sourcePath==oldPath)clip.sourcePath=moved.getFullPathName().toStdString();
    require(json(candidate)==json(expected),"relink changed clip metadata or unrelated source");
    require(missingAudioSources(candidate,formats).size()==1,"missing source still reported");
    require(!prepareAudioRelink(candidate,moved.getFullPathName().toStdString(),shortFile,formats,session,error),
        "readable source must not be replaced through missing workflow");
    require(json(session)==before,"rejecting readable source modified output");
    ClipEditHistory history;require(history.record(session,candidate),"record relink");
    auto live=candidate;require(history.undo(live)==ClipEditHistory::Result::applied && json(live)==before,"relink undo");
    require(history.redo(live)==ClipEditHistory::Result::applied && json(live)==json(candidate),"relink redo");
    const auto project=root.getChildFile("relinked.idwproj");require(saveSession(live,project),"save relink");
    Session restored;require(loadSession(project,restored) && json(restored)==json(live),"relink save/reopen");
    SessionRecovery recovery(root.getChildFile("recovery"));recovery.begin(live,true);
    require(recovery.checkpoint(live,project)==SessionRecovery::Result::saved,"relink recovery save");
    juce::File origin;require(SessionRecovery::read(recovery.copies().front(),restored,origin) &&
        json(restored)==json(live),"relink recovery round trip");
    // Keep the unrelated missing source for model tests above; omit that track
    // when exporting because export correctly refuses missing media.
    live.tracks.pop_back();
    const auto output=root.getChildFile("relinked.wav");
    require(renderSessionWav(live,output,error),"relinked export");
    auto exported=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(output));
    require(exported!=nullptr,"export reader");
    juce::AudioBuffer<float> expectedAudio(2,192);require(exported->read(&expectedAudio,0,192,0,true,true),"export samples");
    for(int block:{1,17,64,256}){
        juce::AudioFormatReaderSource source(formats.createReaderFor(moved),true);
        juce::AudioTransportSource transport;transport.setSource(&source,0,nullptr,44100);
        transport.prepareToPlay(block,44100);transport.start();
        juce::AudioBuffer<float> scratch(2,block),actual(2,192);actual.clear();juce::int64 next=-1;
        for(int pos=0;pos<192;pos+=block)for(const auto& clip:live.tracks[0].clips){
            const auto window=readClipWindow(transport,scratch,next,clip,60,44100,pos,std::min(block,192-pos));
            for(int ch=0;ch<2;++ch)actual.addFrom(ch,pos+window.offset,scratch,ch,0,window.frames);
        }
        require(actual.getMagnitude(0,192)>0.03f,"relinked playback silent");
        for(int ch=0;ch<2;++ch)for(int i=0;i<192;++i)
            require(std::abs(actual.getSample(ch,i)-expectedAudio.getSample(ch,i))<0.00001f,"relink playback/export mismatch");
        transport.setSource(nullptr);transport.releaseResources();
    }
    exported.reset();juce::MemoryBlock finalBytes;
    require(moved.loadFileAsData(finalBytes) && finalBytes==originalBytes && !old.exists(),"relink altered source audio");
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS: relink validation, metadata preservation, history, persistence, recovery and WAV playback/export\n";
}
