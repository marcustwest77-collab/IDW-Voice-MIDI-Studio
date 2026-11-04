#include "NewProject.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static juce::String text(const Session& s){return juce::JSON::toString(sessionToJson(s),true);}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-new-project","",false);
    try{
        require(root.createDirectory().wasOk(),"fixture folder");
        const auto original=root.getChildFile("saved.idwproj"),audio=root.getChildFile("take.wav");
        require(audio.replaceWithText("preserve source audio bytes"),"audio fixture");
        Session session;session.addTrack(TrackType::audio);session.addTrack(TrackType::midi);
        require(session.addClip(0,{"take.wav",0,4,0.5}),"clip fixture");
        require(session.addMidiNote(1,{60,2,1,70}),"note fixture");
        require(saveSession(session,original),"saved project");
        const auto savedBytes=original.loadFileAsString(),audioBytes=audio.loadFileAsString();
        auto project=original;
        SessionRecovery recovery(root.getChildFile("recovery"));recovery.begin(session);
        session.setTempo(93);session.masterMix.gain=0.5;
        const auto edited=text(session);
        NewProjectPlan plan(session,project);juce::String error;
        require(!plan.commit(session,project,recovery,false,true,error) && error.isEmpty(),"cancel");
        require(text(session)==edited && project==original && recovery.copies().empty(),"cancel mutated state");
        require(!plan.commit(session,project,recovery,true,false,error) && error.isNotEmpty(),"busy accepted");
        require(text(session)==edited && recovery.copies().empty(),"busy mutated state");
        session.masterMix.gain=0.75;
        const auto newer=text(session);
        require(!plan.commit(session,project,recovery,true,true,error) && text(session)==newer,"stale confirmation overwrote edit");
        NewProjectPlan pathPlan(session,project);project=root.getChildFile("another.idwproj");
        require(!pathPlan.commit(session,project,recovery,true,true,error),"changed document origin accepted");
        project=original;
        const auto blocker=root.getChildFile("blocked");require(blocker.replaceWithText("not a directory"),"failure fixture");
        SessionRecovery failedRecovery(blocker);failedRecovery.begin(session,true);
        NewProjectPlan failedPlan(session,project);
        require(!failedPlan.commit(session,project,failedRecovery,true,true,error) && error.isNotEmpty(),"recovery write failure accepted");
        require(text(session)==newer && project==original,"write failure lost live work");
        // The same confirmation is retryable after recovery storage becomes available.
        require(blocker.deleteFile(),"remove blocker");
        require(failedPlan.commit(session,project,failedRecovery,true,true,error),"retry failed");
        Session expected;expected.addTrack(TrackType::audio);expected.addTrack(TrackType::midi);
        require(text(session)==text(expected) && project==juce::File{},"new project defaults or origin");
        require(!failedPlan.commit(session,project,failedRecovery,true,true,error),"double commit");
        const auto copies=failedRecovery.copies();require(copies.size()==1,"old work not retained");
        Session recovered;juce::File recoveredOrigin;
        require(SessionRecovery::read(copies.front(),recovered,recoveredOrigin),"recovery unreadable");
        Session old;require(sessionFromJson(juce::JSON::parse(newer),old),"expected recovery parse");
        old.tracks[0].clips[0].sourcePath=audio.getFullPathName().toStdString();
        require(text(recovered)==text(old) && recoveredOrigin==original,"notes mix timing or media recovery changed");
        require(failedRecovery.checkpoint(session,project)==SessionRecovery::Result::unchanged,"fresh project dirty");
        require(failedRecovery.markSaved(session) && copies.front().existsAsFile(),"new save erased previous document recovery");
        require(original.loadFileAsString()==savedBytes && audio.loadFileAsString()==audioBytes,"original project or audio modified");
        // Clean named projects require no redundant recovery copy; the file itself remains intact.
        Session clean;require(loadSession(original,clean),"reload original");project=original;
        SessionRecovery cleanRecovery(root.getChildFile("clean"));cleanRecovery.begin(clean);
        NewProjectPlan cleanPlan(clean,project);
        require(cleanPlan.commit(clean,project,cleanRecovery,true,true,error) && cleanRecovery.copies().empty(),"clean project switch");
        // Unsaved MIDI work can be recovered without a named project.
        clean.addMidiNote(1,{67,0,1,99});NewProjectPlan untitled(clean,project);
        require(untitled.commit(clean,project,cleanRecovery,true,true,error),"untitled switch");
        const auto unsaved=cleanRecovery.copies();require(unsaved.size()==1,"untitled copy");
        require(SessionRecovery::read(unsaved.front(),recovered,recoveredOrigin) && recoveredOrigin==juce::File{} &&
            recovered.tracks[1].notes.size()==1 && recovered.tracks[1].notes[0].velocity==99,"untitled recovery");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"fixture cleanup");
        std::cout<<"New project lifecycle tests passed\n";return 0;
    }catch(const std::exception& e){
        if(root.getParentDirectory()==temp)root.deleteRecursively();
        std::cerr<<e.what()<<'\n';return 1;
    }
}
