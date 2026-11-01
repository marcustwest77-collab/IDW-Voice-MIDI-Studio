#include "SessionPersistence.h"
#include <iostream>
#include <stdexcept>

using namespace idw::daw;
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static juce::MemoryBlock bytes(const juce::File& file) {
    juce::MemoryBlock result;require(file.loadFileAsData(result),"read saved bytes");return result;
}
static bool write(const juce::File& file,const juce::String& text) {
    auto output=file.createOutputStream();if(!output)return false;
    const bool ok=output->write(text.toRawUTF8(),static_cast<std::size_t>(text.getNumBytesAsUTF8()));
    output->flush();return ok && output->getStatus().wasOk();
}
int main() {
    try {
        const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile("idw-validated-save","",false);
        require(root.createDirectory().wasOk(),"fixture directory");
        const auto project=root.getChildFile("Session.idwproj");
        Session session;session.setTempo(120);require(session.addTrack(TrackType::audio),"audio track");
        session.tracks[0].name=u8"Vocals \u03b2 \U0001F3B5 "+std::string(12000,'x');
        require(session.addClip(0,{"missing-source.wav",1,4,0.25}),"missing-media reference");
        require(session.addTrack(TrackType::midi) && session.addMidiNote(1,{64,2,1,95}),"MIDI fixture");
        require(saveSession(session,project),"initial checked save");
        Session loaded;require(loadSession(project,loaded) &&
            juce::JSON::toString(sessionToJson(loaded))==juce::JSON::toString(sessionToJson(session)),
            "Unicode/large metadata/missing media/MIDI roundtrip");
        const auto original=bytes(project);
        juce::File candidate;
        for(int failure=0;failure<5;++failure) {
            const bool saved=persistence_detail::saveValidated(session,project,
                [&](const juce::File& staged,const juce::String& text) {
                    candidate=staged;
                    require(bytes(project)==original,"destination changed before candidate write");
                    if(failure==0)return false; // Open/write failure with no output.
                    if(failure==1){require(write(staged,"{partial"),"partial fixture");return false;}
                    if(failure==2){require(write(staged,"{partial"),"truncated fixture");return true;}
                    auto wrong=juce::JSON::parse(text);
                    if(failure==3)wrong.getDynamicObject()->setProperty("bpm",121.0);
                    else {
                        auto* track=wrong.getProperty("tracks",{}).getArray()->getReference(0).getDynamicObject();
                        const auto name=track->getProperty("name").toString();
                        track->setProperty("name",name.dropLastCharacters(1)+"y"); // Beyond the first readback block.
                    }
                    const auto changed=juce::JSON::toString(wrong,true);
                    require(changed.getNumBytesAsUTF8()==text.getNumBytesAsUTF8(),"same-size corruption fixture");
                    require(write(staged,changed),"different snapshot fixture");return true;
                });
            require(!saved && bytes(project)==original && !candidate.exists(),
                    "failed/partial/wrong candidate replaced project or leaked temporary output");
        }
        bool threw=false;
        try {
            persistence_detail::saveValidated(session,project,
                [&](const juce::File& staged,const juce::String&) -> bool {
                    candidate=staged;require(write(staged,"{partial"),"throw fixture");
                    throw std::runtime_error("injected staging exception");
                });
        } catch(const std::runtime_error& error) {threw=std::string(error.what())=="injected staging exception";}
        require(threw && bytes(project)==original && !candidate.exists(),"exception lost project or staging cleanup");
        for(int invalid=0;invalid<3;++invalid) {
            auto bad=session;
            if(invalid==0)bad.bpm=0;
            if(invalid==1)bad.tracks[0].gain=3.0f; // Loading would silently clamp this.
            if(invalid==2)bad.tracks[0].clips[0].lengthBeats=0;
            require(!saveSession(bad,project) && bytes(project)==original,"invalid model overwrote valid save");
        }
        const auto directory=root.getChildFile("not-a-project.idwproj");
        require(directory.createDirectory().wasOk() && directory.getChildFile("keep.txt").replaceWithText("keep"),
                "directory fixture");
        require(!saveSession(session,directory) && directory.getChildFile("keep.txt").loadFileAsString()=="keep",
                "directory destination changed");
        const auto parentFile=root.getChildFile("not-a-folder");require(parentFile.replaceWithText("keep"),"blocked parent");
        require(!saveSession(session,parentFile.getChildFile("Session.idwproj")) &&
                parentFile.loadFileAsString()=="keep","invalid parent changed");
        session.setTempo(137.5);
        require(persistence_detail::saveValidated(session,project,
            [&](const juce::File& staged,const juce::String& text) {
                require(bytes(project)==original,"old save missing during staging");
                const bool ok=write(staged,text);
                require(bytes(project)==original,"old save changed before validated replacement");
                return ok;
            }),"valid replacement failed");
        require(loadSession(project,loaded) && loaded.bpm==137.5 && bytes(project)!=original,"replacement unreadable");
        require(saveSession(session,project),"repeat public save");
        require(root.findChildFiles(juce::File::findFiles,false,"*temp*").isEmpty(),"temporary save files leaked");
        require(root.deleteRecursively(),"fixture cleanup");
        std::cout << "PASS: validated save preserves prior project on failed/partial/wrong writes and invalid models\n";
        return 0;
    } catch(const std::exception& error) {std::cerr << "FAIL: " << error.what() << '\n';return 1;}
}
