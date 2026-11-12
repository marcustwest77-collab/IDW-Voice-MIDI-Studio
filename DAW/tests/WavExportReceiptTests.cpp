#include "WavExportReceipt.h"
#include "OfflineBounce.h"
#include "SessionPersistence.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-export-receipt","",false);
    try{
        require(root.createDirectory().wasOk(),"fixture folder");
        Session session;session.bpm=120;session.addTrack(TrackType::midi);
        session.tracks[0].notes={{60,0,1,30},{64,3,1,70}};
        session.playbackLoopEnabled=true;session.playbackLoop={3,4};
        const auto before=juce::JSON::toString(sessionToJson(session),true);
        const auto source=root.getChildFile("Current Mix.idwproj");
        for(int rate:{44100,48000}){
            const auto output=root.getChildFile("Mix "+juce::String(rate)+".wav");juce::String error;
            require(renderProjectWav(session,output,error,rate),"render receipt fixture");
            juce::MemoryBlock bytes;require(output.loadFileAsData(bytes),"read original bytes");
            const auto receipt=wavExportReceipt(source,output);
            require(receipt && receipt->contains(source.getFullPathName()) && receipt->contains(output.getFullPathName()) &&
                receipt->contains("current session snapshot") && receipt->contains("Duration: 2.000 seconds") &&
                receipt->contains("2 channels | 24-bit | "+juce::String(rate)+" Hz"),"receipt source/destination/actual metadata");
            const auto untitled=wavExportReceipt({},output);
            require(untitled && untitled->contains("Untitled project"),"untitled source label");
            juce::MemoryBlock after;require(output.loadFileAsData(after) && after==bytes,"receipt modified export");
            after.reset();
            require(!renderProjectWav(session,output,error,rate) && output.loadFileAsData(after) && after==bytes,"existing export changed");
        }
        require(juce::JSON::toString(sessionToJson(session),true)==before,"receipt changed session/loop");
        require(!wavExportReceipt(source,root.getChildFile("Missing.wav")),"missing output receipt");
        const auto invalid=root.getChildFile("Invalid.wav");require(invalid.replaceWithText("not a WAV"),"invalid fixture");
        require(!wavExportReceipt(source,invalid),"invalid output receipt");
        // The receipt reads the completed file, not the later editable session.
        session.bpm=93;const auto fractional=root.getChildFile("Fractional.wav");juce::String error;
        require(renderProjectWav(session,fractional,error,44100),"fractional fixture");
        session.bpm=20;const auto receipt=wavExportReceipt(source,fractional);
        require(receipt && receipt->contains("Duration: 2.581 seconds"),"duration inferred from mutable session");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"WAV export receipt tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
