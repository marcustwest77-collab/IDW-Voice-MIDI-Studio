#include "ProjectPackaging.h"
#include "OfflineBounce.h"
#include <iostream>
#include <stdexcept>

using namespace idw::daw;
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static juce::String json(const Session& session) {return juce::JSON::toString(sessionToJson(session));}
static juce::MemoryBlock bytes(const juce::File& file) {
    juce::MemoryBlock result;require(file.loadFileAsData(result),"read source bytes");return result;
}
static void makeWav(const juce::File& file,float level) {
    require(file.getParentDirectory().createDirectory().wasOk(),"source directory");
    auto stream=file.createOutputStream();require(stream!=nullptr,"source stream");
    juce::WavAudioFormat wav;
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(),44100,2,24,{},0));
    require(writer!=nullptr,"source writer");stream.release();
    juce::AudioBuffer<float> buffer(2,44100);
    for(int i=0;i<44100;++i) {
        const float value=level*(i<22050?1.0f:2.0f);
        buffer.setSample(0,i,value);buffer.setSample(1,i,-value*0.5f);
    }
    require(writer->writeFromFloatArrays(buffer.getArrayOfReadPointers(),2,44100),"source samples");
}
static void compareMix(const juce::File& first,const juce::File& second) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> a(formats.createReaderFor(first)),b(formats.createReaderFor(second));
    require(a && b && a->sampleRate==b->sampleRate && a->numChannels==2 && b->numChannels==2 &&
            a->lengthInSamples==b->lengthInSamples,"relocated mix format/duration");
    juce::AudioBuffer<float> x(2,4096),y(2,4096);
    for(juce::int64 position=0;position<a->lengthInSamples;position+=4096) {
        const int count=static_cast<int>(std::min<juce::int64>(4096,a->lengthInSamples-position));
        require(a->read(&x,0,count,position,true,true) && b->read(&y,0,count,position,true,true),"mix read");
        for(int channel=0;channel<2;++channel)for(int i=0;i<count;++i)
            require(x.getSample(channel,i)==y.getSample(channel,i),"relocation changed stereo mix samples");
    }
}
int main(int argc,char** argv) {
    try {
        require(argc<=2,"usage: PortableProjectTests [fresh-evidence-directory]");
        const auto root=argc==2?juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]):
            juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-portable","",false);
        require(!root.exists() && root.createDirectory().wasOk(),"evidence directory must be new");
        const auto originals=root.getChildFile("Originals");
        const auto a=originals.getChildFile("A/take.wav"),b=originals.getChildFile("B/take.wav");
        makeWav(a,0.125f);makeWav(b,-0.0625f);
        const auto aBytes=bytes(a),bBytes=bytes(b);
        Session source;source.setTempo(120);source.addTrack(TrackType::audio);
        require(source.addClip(0,{a.getFullPathName().toStdString(),0,1,0}) &&
                source.addClip(0,{a.getFullPathName().toStdString(),2,1,0.5}),"split/repeated source");
        source.addTrack(TrackType::audio);
        require(source.addClip(1,{b.getFullPathName().toStdString(),0.5,1,0.25}) &&
                source.addClip(1,{a.getFullPathName().toStdString(),3,1,0}),"cross-track source reuse");
        require(source.setTrackMix(1,0.6f,0.25f) && source.setClipGain(0,1,-6),"mix fixture");
        source.addTrack(TrackType::midi);require(source.addMidiNote(2,{64,0,1,95}),"MIDI metadata");
        const auto sourceJson=json(source);juce::String error;
        require(!exportPortableProject(source,{},error) && error.isNotEmpty(),"empty destination accepted");
        const auto reference=root.getChildFile("Reference.wav");
        require(renderSessionWav(source,reference,error),"reference mix");
        const auto package=root.getChildFile("Package");error="stale error";
        require(exportPortableProject(source,package,error),"portable export");
        require(package.getChildFile("Audio").findChildFiles(juce::File::findFiles,false).size()==2,
                "repeated source copied more than once or same-name sources merged");
        require(error.isEmpty() && json(source)==sourceJson && bytes(a)==aBytes && bytes(b)==bBytes,
                "export changed live project/source or retained stale error");
        const auto relocated=root.getChildFile("Relocated/Package");
        require(relocated.getParentDirectory().createDirectory().wasOk() && package.moveFileTo(relocated),"package move");
        const auto hidden=root.getChildFile("Originals-unavailable");
        require(originals.moveFileTo(hidden),"hide original fixture media");
        Session portable;
        require(loadSession(relocated.getChildFile("Session.idwproj"),portable),"reopen relocated project");
        const auto aPath=portable.tracks[0].clips[0].sourcePath,bPath=portable.tracks[1].clips[0].sourcePath;
        require(aPath!=bPath && portable.tracks[0].clips[1].sourcePath==aPath &&
                portable.tracks[1].clips[1].sourcePath==aPath,"portable references do not preserve source identity");
        auto expected=source;
        for(auto& track:expected.tracks)for(auto& clip:track.clips)
            clip.sourcePath=clip.sourcePath==a.getFullPathName().toStdString()?aPath:bPath;
        require(json(portable)==json(expected),"portable export changed clip/mix/MIDI metadata");
        require(bytes(relocated.getChildFile(aPath))==aBytes && bytes(relocated.getChildFile(bPath))==bBytes,
                "portable audio bytes changed");
        const auto relative=portable;
        for(auto& track:portable.tracks)for(auto& clip:track.clips) {
            require(!juce::File::isAbsolutePath(juce::String(clip.sourcePath)) &&
                    juce::String(clip.sourcePath).startsWith("Audio/"),"portable path is not contained/relative");
            clip.sourcePath=relocated.getChildFile(clip.sourcePath).getFullPathName().toStdString();
        }
        const auto mix=root.getChildFile("Relocated.wav");
        require(renderSessionWav(portable,mix,error),"relocated mix needs unavailable originals");compareMix(reference,mix);
        const auto originalPackageBytes=bytes(relocated.getChildFile("Session.idwproj"));
        require(!exportPortableProject(portable,relocated,error) &&
                bytes(relocated.getChildFile("Session.idwproj"))==originalPackageBytes,"existing package overwritten");
        auto missing=portable;missing.tracks[1].clips.back().sourcePath=root.getChildFile("missing.wav").getFullPathName().toStdString();
        const auto failed=root.getChildFile("Failed");
        require(!exportPortableProject(missing,failed,error) && !failed.exists(),"partial missing-media package retained");
        const auto unresolved=root.getChildFile("Unresolved");
        require(!exportPortableProject(relative,unresolved,error) && !unresolved.exists(),"relative input guessed from cwd");
        auto invalid=portable;invalid.bpm=0;const auto invalidFolder=root.getChildFile("Invalid");
        require(!exportPortableProject(invalid,invalidFolder,error) && !invalidFolder.exists(),"invalid project package retained");
        require(bytes(hidden.getChildFile("A/take.wav"))==aBytes && bytes(hidden.getChildFile("B/take.wav"))==bBytes &&
                json(source)==sourceJson,"portable failure paths changed original source/project");
        if(argc==1)require(root.deleteRecursively(),"fixture cleanup");
        else std::cout<<"Synthetic portable evidence: "<<root.getFullPathName()<<'\n';
        std::cout<<"PASS: unique-source copies, basename separation, relocation, exact mix and rollback protection\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
