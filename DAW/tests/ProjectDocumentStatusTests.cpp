#include "ProjectDocumentStatus.h"
#include "NewProject.h"
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static juce::String json(const Session& s){return juce::JSON::toString(sessionToJson(s),true);}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-document-status","",false);
    try{
        require(root.createDirectory().wasOk(),"fixture directory");
        Session session;session.addTrack(TrackType::audio);session.addTrack(TrackType::midi);
        ProjectDocumentStatus status;status.begin(session,{});
        require(status.summary(session)=="Untitled project | Not saved yet","startup status");
        session.addMidiNote(1,{60,0,1,70});
        require(status.hasUnsavedChanges(session) && status.summary(session).contains("Unsaved changes"),"untitled edit");
        const auto project=root.getChildFile("My Song.idwproj");
        require(saveProjectSession(session,project),"manual save");status.begin(session,project);
        require(status.summary(session)=="My Song.idwproj | Saved" && status.location()==project.getFullPathName(),"save name and full path");
        const auto saved=session;SessionRecovery recovery(root.getChildFile("recovery"));recovery.begin(session);
        session.tracks[1].notes[0].velocity=40;
        require(status.hasUnsavedChanges(session),"velocity edit not dirty");
        require(recovery.checkpoint(session,project)==SessionRecovery::Result::saved &&
            status.hasUnsavedChanges(session),"autosave falsely marked manual save");
        session=saved;require(!status.hasUnsavedChanges(session),"undo to saved state remains dirty");
        session.tracks[1].notes[0].velocity=40;require(status.hasUnsavedChanges(session),"redo not dirty");
        Session loaded;require(loadSession(project,loaded),"reopen saved document");status.begin(loaded,project);
        require(!status.hasUnsavedChanges(loaded),"opened project dirty");
        Session recovered;juce::File origin;
        require(SessionRecovery::read(recovery.copies().front(),recovered,origin),"read recovery");
        status.begin(recovered,origin,true);
        require(status.hasUnsavedChanges(recovered) && status.summary(recovered)=="My Song.idwproj | Recovered - save required","recovery falsely saved");
        require(recovery.checkpoint(recovered,origin)!=SessionRecovery::Result::failed && status.hasUnsavedChanges(recovered),"recovery checkpoint cleared flag");
        const auto copy=root.getChildFile("Recovered Song.idwproj");
        require(saveProjectSession(recovered,copy),"save recovered copy");status.begin(recovered,copy);
        require(status.summary(recovered)=="Recovered Song.idwproj | Saved","save recovery clears warning");
        NewProjectPlan newProject(recovered,copy);auto path=copy;juce::String error;
        require(newProject.commit(recovered,path,recovery,true,true,error),"new project switch");status.begin(recovered,path);
        require(status.summary(recovered)=="Untitled project | Not saved yet","new project retains filename or dirty state");
        status.begin(recovered,{},true);
        require(status.summary(recovered)=="Untitled project | Recovered - save required","untitled recovery");
        // Every currently persisted field must affect document equality, regardless
        // of which UI, import, history or remote command changes it.
        Session base=saved;base.addClip(0,{"/source.wav",0,4,0});
        const std::vector<std::function<void(Session&)>> edits={
            [](auto& s){s.bpm=93;},[](auto& s){s.numerator=3;},[](auto& s){s.denominator=8;},
            [](auto& s){s.masterMix.gain=0.5;},[](auto& s){s.masterMix.ceilingDb=-3;},
            [](auto& s){s.masterMix.lowDb=-2;},[](auto& s){s.masterMix.highDb=2;},
            [](auto& s){s.addTrack(TrackType::audio);},[](auto& s){s.tracks.erase(s.tracks.begin());},
            [](auto& s){std::swap(s.tracks[0],s.tracks[1]);},
            [](auto& s){s.tracks[0].name="Renamed";},[](auto& s){s.tracks[0].type=TrackType::midi;},
            [](auto& s){s.tracks[0].mute=true;},[](auto& s){s.tracks[0].solo=true;},
            [](auto& s){s.tracks[0].inputChannel=1;},[](auto& s){s.tracks[0].gain=0.5f;},
            [](auto& s){s.tracks[0].pan=-0.5f;},[](auto& s){s.tracks[1].synthPreset=2;},
            [](auto& s){s.tracks[1].synthAttackMs=12;},[](auto& s){s.tracks[1].synthReleaseMs=20;},
            [](auto& s){s.tracks[0].clips.clear();},[](auto& s){s.tracks[0].clips[0].sourcePath="/other.wav";},
            [](auto& s){s.tracks[0].clips[0].startBeat=1;},[](auto& s){s.tracks[0].clips[0].lengthBeats=3;},
            [](auto& s){s.tracks[0].clips[0].sourceOffsetSeconds=0.5;},
            [](auto& s){s.tracks[0].clips[0].fadeInBeats=0.5;},[](auto& s){s.tracks[0].clips[0].fadeOutBeats=1;},
            [](auto& s){s.tracks[0].clips[0].fadeInCurve=FadeCurve::equalPower;},
            [](auto& s){s.tracks[0].clips[0].fadeOutCurve=FadeCurve::equalPower;},
            [](auto& s){s.tracks[0].clips[0].gainDb=-6;},[](auto& s){s.tracks[1].notes.clear();},
            [](auto& s){s.tracks[1].notes[0].pitch=64;},[](auto& s){s.tracks[1].notes[0].startBeat=2;},
            [](auto& s){s.tracks[1].notes[0].lengthBeats=2;},[](auto& s){s.tracks[1].notes[0].velocity=40;}
        };
        status.begin(base,project);
        for(const auto& edit:edits){auto changed=base;edit(changed);
            require(json(base)!=json(changed) && !sameDocumentState(base,changed) && !sameDocumentState(changed,base) &&
                status.hasUnsavedChanges(changed),"persisted field omitted from dirty detection");}
        auto ignored=base;ignored.tracks[0].synthPreset=4;ignored.tracks[0].synthAttackMs=50;
        ignored.tracks[0].synthReleaseMs=50;ignored.tracks[1].inputChannel=7;
        require(json(base)==json(ignored) && sameDocumentState(base,ignored),"non-persisted track properties made dirty");
        // Track/note traversal compares content, not vector storage or object identity.
        auto large=base;large.tracks[1].notes.assign(50000,{60,0,1,70});status.begin(large,project);
        require(!status.hasUnsavedChanges(large),"large unchanged document");
        large.tracks[1].notes.back().velocity=71;require(status.hasUnsavedChanges(large),"last note edit omitted");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"Project document status tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
