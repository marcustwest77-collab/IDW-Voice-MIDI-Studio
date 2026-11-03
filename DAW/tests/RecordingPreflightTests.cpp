#include "RecordingPreflight.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MemoryBlock bytes(const juce::File& f){juce::MemoryBlock b;require(f.loadFileAsData(b),"read bytes");return b;}
static bool write(const juce::File& f,const juce::String& text){
    auto output=f.createOutputStream();if(!output)return false;
    const bool ok=output->write(text.toRawUTF8(),(size_t)text.getNumBytesAsUTF8());output->flush();return ok && output->getStatus().wasOk();
}
int main(){try{
    const auto good=assessRecordingDevice(true,true,48000,48000,true);
    require(good.captureReady() && good.outputActive && good.sampleRate==48000,"open matched device rejected");
    require(!assessRecordingDevice(true,false,48000,48000,true).captureReady(),"closed device passed with stale rate");
    require(!assessRecordingDevice(false,true,48000,48000,true).captureReady(),"missing device passed");
    require(!assessRecordingDevice(true,true,48000,44100,true).captureReady(),"engine/device rate mismatch passed");
    for(double invalid:{0.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        require(!assessRecordingDevice(true,true,invalid,48000,true).captureReady(),"invalid live rate passed");
        require(!assessRecordingDevice(true,true,48000,invalid,true).captureReady(),"invalid engine rate passed");
    }
    require(!inspectRecordingDevice(nullptr,48000).captureReady(),"null device inspection passed");
    RecordingPreflightChecks ready;ready.audioTrack=true;ready.armed=true;ready.emptyTrack=true;ready.inputReady=true;
    ready.folderWritable=true;ready.device=good;ready.freeBytes=100LL*1024*1024;
    require(ready.passes(),"valid preflight rejected");
    for(int field=0;field<9;++field){
        auto bad=ready;
        if(field==0)bad.audioTrack=false;if(field==1)bad.armed=false;if(field==2)bad.emptyTrack=false;
        if(field==3)bad.inputReady=false;if(field==4)bad.folderWritable=false;if(field==5)bad.device.open=false;
        if(field==6)bad.device.rateMatches=false;if(field==7)bad.device.outputActive=false;if(field==8)--bad.freeBytes;
        require(!bad.passes(),"incomplete preflight reported PASS");
    }
    auto unknown=ready;unknown.freeBytes=-1;require(!unknown.passes(),"unknown free space passed");
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-recording-preflight","",false);
    require(root.createDirectory().wasOk(),"fixture directory");const auto folder=root.getChildFile("Recordings");
    require(folder.createDirectory().wasOk(),"recording fixture directory");
    const auto original=folder.getChildFile("existing-take.wav");require(original.replaceWithText("existing take bytes must stay unchanged"),"source fixture");
    const auto saved=bytes(original);
    require(probeRecordingFolder(folder).writable,"writable folder probe failed");
    require(folder.findChildFiles(juce::File::findFiles,false).size()==1 && bytes(original)==saved,"probe left a marker or changed take");
    const auto fail=[&](auto writer){
        const auto result=preflight_detail::probeFolder(folder,writer);
        require(!result.writable && result.detail.isNotEmpty() && folder.findChildFiles(juce::File::findFiles,false).size()==1 && bytes(original)==saved,
                "failed probe passed, leaked a marker or modified take");};
    fail([](const juce::File&,const juce::String&){return false;});
    fail([](const juce::File& f,const juce::String& text){return write(f,text.substring(0,5));});
    fail([](const juce::File& f,const juce::String& text){auto bad=text.toStdString();bad.back()='!';return write(f,juce::String(bad));});
    fail([](const juce::File&,const juce::String&)->bool{throw std::runtime_error("simulated write failure");});
    const auto missing=root.getChildFile("new/Recordings");require(probeRecordingFolder(missing).writable && missing.isDirectory() &&
        missing.findChildFiles(juce::File::findFiles,false).isEmpty(),"new recording directory probe");
    const auto blocker=root.getChildFile("blocked");require(blocker.replaceWithText("keep"),"blocked fixture");
    require(!probeRecordingFolder(blocker).writable && !probeRecordingFolder(blocker.getChildFile("Recordings")).writable && blocker.loadFileAsString()=="keep",
        "blocked folder probe changed existing file");
    require(!probeRecordingFolder(juce::File{}).writable,"empty destination accepted");
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS recording preflight: open/live-rate requirements, complete configuration matrix, exact temporary write/flush/readback/cleanup and unchanged existing takes; no real drivers\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
