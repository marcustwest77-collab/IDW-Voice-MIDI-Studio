#include "ProjectSaveDestination.h"
#include "ProjectDocumentStatus.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-project-save-path","",false);
    try{
        require(root.createDirectory().wasOk(),"fixture folder");
        const auto canonical=root.getChildFile("Song.v2.idwproj");
        for(const auto* name:{"Song.v2","Song.v2.idwproj","Song.v2.IDWPROJ","Song.v2.idwproj.idwproj",
            "Song.v2.idwproj..idwproj","Song.v2.idwproj..."}){
            const auto normalized=projectSaveDestination(root.getChildFile(name));
            require(normalized==canonical && projectSaveDestination(normalized)==normalized,"suffix normalization or idempotence");
        }
        require(projectSaveDestination({})==juce::File{},"cancel path");
        require(projectSaveDestination(root.getChildFile(".idwproj"))==juce::File{},"empty filename");
        const auto unicode=root.getChildFile(juce::String::fromUTF8("Vocals \xce\xb2.idwproj"));
        require(projectSaveDestination(unicode)==unicode,"Unicode path changed");
        require(suggestedProjectSaveDestination({},root)==root.getChildFile("IDW Session.idwproj"),"untitled suggestion");
        require(suggestedProjectSaveDestination(canonical,root.getChildFile("elsewhere"))==canonical,"existing project location lost");
        require(suggestedProjectSaveDestination(root.getChildFile("Song.v2.idwproj..idwproj"),root)==canonical,"suggestion kept repeated extension");
        Session session;session.addTrack(TrackType::audio);session.addTrack(TrackType::midi);
        session.addMidiNote(1,{60,0,1,70});
        const auto audio=root.getChildFile("take.wav");require(audio.replaceWithText("preserve audio bytes"),"audio fixture");
        session.addClip(0,{audio.getFullPathName().toStdString(),0,4,0});
        require(saveProjectSession(session,canonical),"initial saved project");
        const auto original=canonical.loadFileAsString();
        auto edited=session;edited.setTempo(100);ProjectDocumentStatus status;status.begin(session,canonical);
        ProjectSavePlan replace(edited,canonical,root.getChildFile("Song.v2.idwproj..idwproj"));
        juce::String error;
        require(replace.target()==canonical,"overwrite prompt path differs from write target");
        require(!replace.commit(edited,canonical,true,false,error) && error.isNotEmpty(),"normalization bypassed replace consent");
        require(canonical.loadFileAsString()==original && status.hasUnsavedChanges(edited),"unapproved save modified file or status");
        require(!replace.commit(edited,canonical,false,true,error) && canonical.loadFileAsString()==original,"busy save accepted");
        auto changed=edited;changed.tracks[1].notes[0].velocity=40;
        require(!replace.commit(changed,canonical,true,true,error) && canonical.loadFileAsString()==original,"stale confirmation overwrote newer edit");
        require(!replace.commit(edited,root.getChildFile("another.idwproj"),true,true,error),"changed document path accepted");
        require(replace.commit(edited,canonical,true,true,error),"approved replace failed");
        status.begin(edited,replace.target());require(!status.hasUnsavedChanges(edited),"saved status");
        Session read;require(loadSession(canonical,read) && read.bpm==100 && read.tracks[1].notes[0].velocity==70,"saved model mismatch");
        require(!replace.commit(edited,canonical,true,true,error),"double commit");
        // A new file appearing after selection still needs explicit consent.
        const auto next=root.getChildFile("New.idwproj");ProjectSavePlan race(edited,canonical,next);
        require(next.replaceWithText("appeared after picker"),"new target fixture");
        require(!race.commit(edited,canonical,true,false,error) && next.loadFileAsString()=="appeared after picker","late target overwritten");
        const auto folder=root.getChildFile("Folder.idwproj");require(folder.createDirectory().wasOk(),"directory target");
        ProjectSavePlan directory(edited,canonical,folder);
        require(!directory.commit(edited,canonical,true,true,error) && folder.isDirectory(),"directory replaced");
        // A blocked parent deterministically fails; a retry is possible after repair.
        const auto blocker=root.getChildFile("blocked");require(blocker.replaceWithText("not a folder"),"blocked parent");
        const auto retryTarget=blocker.getChildFile("Retry.idwproj");ProjectSavePlan retry(edited,canonical,retryTarget);
        const auto beforeFailure=canonical.loadFileAsString();
        require(!retry.commit(edited,canonical,true,false,error) && canonical.loadFileAsString()==beforeFailure,"save failure altered saved project");
        require(blocker.deleteFile() && blocker.createDirectory().wasOk(),"repair parent");
        require(retry.commit(edited,canonical,true,false,error) && retryTarget.existsAsFile(),"save failure not retryable");
        require(audio.loadFileAsString()=="preserve audio bytes","audio changed");
        require(!root.getChildFile("Song.v2.idwproj..idwproj").exists(),"alias file created");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"Project save destination tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
