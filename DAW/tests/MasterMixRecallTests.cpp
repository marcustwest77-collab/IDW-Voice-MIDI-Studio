#include "SessionRecovery.h"
#include "ProjectPackaging.h"
#include "OfflineBounce.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::String json(const Session& s){return juce::JSON::toString(sessionToJson(s));}
static bool same(const MasterMix& a,const MasterMix& b){
    return a.gain==b.gain && a.ceilingDb==b.ceilingDb && a.lowDb==b.lowDb && a.highDb==b.highDb;
}
static juce::AudioBuffer<float> read(const juce::File& f){
    juce::WavAudioFormat wav;
    auto r=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(f.createInputStream().release(),true));
    require(r && r->sampleRate==48000 && r->numChannels==2 && r->lengthInSamples==12000,"bounce metadata");
    juce::AudioBuffer<float> buffer(2,12000);require(r->read(&buffer,0,12000,0,true,true),"bounce read");return buffer;
}
static void compare(const juce::File& a,const juce::File& b){
    const auto first=read(a),second=read(b);
    for(int ch=0;ch<2;++ch)for(int i=0;i<12000;++i)
        require(std::abs(first.getSample(ch,i)-second.getSample(ch,i))<0.0000001f,"recalled master changed mix samples");
}
int main(){try{
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("idw-master-recall","",false);
    require(root.createDirectory().wasOk(),"fixture directory");
    Session s;s.bpm=60;s.masterMix={0.63,-2.0,-3.5,4.0};
    s.addTrack(TrackType::audio);s.addTrack(TrackType::midi);
    const auto source=root.getChildFile("source.wav");
    juce::WavAudioFormat wav;auto stream=source.createOutputStream();
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(),48000,2,24,{},0));
    require(writer!=nullptr,"source writer");stream.release();
    juce::AudioBuffer<float> audio(2,9600);
    for(int i=0;i<9600;++i){audio.setSample(0,i,0.125f);audio.setSample(1,i,-0.0625f);}
    require(writer->writeFromAudioSampleBuffer(audio,0,9600),"write source");writer.reset();
    juce::MemoryBlock sourceBytes;require(source.loadFileAsData(sourceBytes),"source bytes");
    require(s.addClip(0,{source.getFullPathName().toStdString(),0,0.2,0}) &&
            s.addMidiNote(1,{69,0,0.25,100}),"audio/MIDI fixture");
    const auto project=root.getChildFile("Session.idwproj");
    require(saveProjectSession(s,project),"save master project");Session reopened;
    require(loadSession(project,reopened) && same(reopened.masterMix,s.masterMix),"reopen master settings");
    // Resolve the real project-relative path as the Workstation does.
    for(auto& t:reopened.tracks)for(auto& c:t.clips)
        c.sourcePath=project.getParentDirectory().getChildFile(juce::String(c.sourcePath)).getFullPathName().toStdString();
    juce::String error;const auto before=root.getChildFile("before.wav"),after=root.getChildFile("after.wav");
    require(renderProjectWav(s,before,error,48000) && renderProjectWav(reopened,after,error,48000),"recalled project export");
    compare(before,after);
    const auto reference=root.getChildFile("explicit.wav");
    require(renderSessionWav(s,reference,error,48000,0.63f,-2.0f,-3.5f,4.0f),"explicit master reference");
    compare(before,reference);
    auto silent=s;silent.masterMix.gain=0;const auto silence=root.getChildFile("silence.wav");
    require(renderProjectWav(silent,silence,error,48000) && read(silence).getMagnitude(0,12000)==0,"master mute ignored");
    auto limited=s;limited.masterMix={1.5,-6,12,12};const auto ceiling=root.getChildFile("ceiling.wav");
    require(renderProjectWav(limited,ceiling,error,48000),"limited export");
    const float peak=read(ceiling).getMagnitude(0,12000),limit=std::pow(10.0f,-6.0f/20);
    require(std::abs(peak-limit)<0.000001f,"recalled ceiling ignored");
    SessionRecovery recovery(root.getChildFile("recovery"));recovery.begin(s);
    auto adjusted=s;adjusted.masterMix.highDb=-7.5;
    require(recovery.checkpoint(adjusted,project)==SessionRecovery::Result::saved,"master-only change not recovered");
    require(recovery.copies().size()==1,"recovery copy");Session recovered;juce::File origin;
    require(SessionRecovery::read(recovery.copies()[0],recovered,origin) && origin==project &&
            same(recovered.masterMix,adjusted.masterMix),"recovery lost master settings");
    const auto package=root.getChildFile("portable");require(exportPortableProject(adjusted,package,error),"portable master export");
    Session portable;require(loadSession(package.getChildFile("Session.idwproj"),portable) &&
            same(portable.masterMix,adjusted.masterMix),"portable lost master settings");
    // Legacy projects reset every master setting to historical defaults, even
    // when the destination session previously had a different master mix.
    auto legacy=sessionToJson(s);legacy.getDynamicObject()->removeProperty("masterMix");
    reopened=s;require(sessionFromJson(legacy,reopened) && same(reopened.masterMix,MasterMix{}),"legacy defaults leaked previous project");
    const auto accepted=[&](MasterMix m){auto trial=s;trial.masterMix=m;Session out;
        require(sessionFromJson(sessionToJson(trial),out) && same(out.masterMix,m),"valid master boundary rejected");};
    accepted({0,-12,-12,-12});accepted({1.5,-0.1,12,12});accepted({0.731245,-3.123456,1.234567,-2.345678});
    // Strict type/range validation preserves the caller's existing session.
    int rejected=0;
    const auto reject=[&](const juce::var& data){Session out=s;const auto unchanged=json(out);
        require(!sessionFromJson(data,out) && json(out)==unchanged,"invalid master accepted or output mutated");++rejected;};
    for(const char* field:{"gain","ceilingDb","lowDb","highDb"}){
        for(const auto& value:std::vector<juce::var>{true,"1.0",juce::var{},juce::var(juce::Array<juce::var>{}),
                1e300,-1e300,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
            auto data=sessionToJson(s);data.getDynamicObject()->getProperty("masterMix").getDynamicObject()->setProperty(field,value);reject(data);
        }
        auto missing=sessionToJson(s);missing.getDynamicObject()->getProperty("masterMix").getDynamicObject()->removeProperty(field);reject(missing);
    }
    for(const auto& value:std::vector<juce::var>{true,"master",juce::var{},juce::var(juce::Array<juce::var>{})}){
        auto data=sessionToJson(s);data.getDynamicObject()->setProperty("masterMix",value);reject(data);
    }
    for(const auto& pair:std::vector<std::pair<const char*,double>>{{"gain",-0.001},{"gain",1.50000001},
            {"ceilingDb",-12.000001},{"ceilingDb",-0.09999999},{"lowDb",-12.000001},{"highDb",12.000001}}){
        auto data=sessionToJson(s);data.getDynamicObject()->getProperty("masterMix").getDynamicObject()->setProperty(pair.first,pair.second);reject(data);
    }
    juce::MemoryBlock projectBytes,afterFailure;require(project.loadFileAsData(projectBytes),"saved bytes");
    auto invalid=s;invalid.masterMix.gain=2;
    require(!saveProjectSession(invalid,project) && project.loadFileAsData(afterFailure) && afterFailure==projectBytes,
            "invalid master overwrote saved project");
    const auto failed=root.getChildFile("invalid.wav");require(!renderProjectWav(invalid,failed,error) && !failed.exists(),"invalid export created output");
    juce::MemoryBlock finalSource;require(source.loadFileAsData(finalSource) && finalSource==sourceBytes,"source audio changed");
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS master mix save/reopen, sample parity, gain/ceiling, recovery, portable, legacy defaults, "<<rejected<<" strict invalid cases and unchanged saved/source files\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
