#include "TrackRename.h"
#include "ProjectDocumentStatus.h"
#include "MidiEditHistory.h"
#include "SessionHistory.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static juce::String json(const Session& s){return juce::JSON::toString(sessionToJson(s),true);}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-track-rename","",false);
    try{
        require(root.createDirectory().wasOk(),"fixture folder");
        const auto project=root.getChildFile("Saved.idwproj"),audio=root.getChildFile("take.wav");
        require(audio.replaceWithText("preserve source bytes"),"audio fixture");
        Session s;s.addTrack(TrackType::audio);s.addTrack(TrackType::midi);
        s.addClip(0,{audio.getFullPathName().toStdString(),0,4,0});s.addMidiNote(1,{60,0,1,70});
        s.masterMix.gain=0.5;s.tracks[0].inputChannel=1;s.tracks[1].synthPreset=2;
        require(saveProjectSession(s,project),"saved fixture");const auto saved=project.loadFileAsString();
        ProjectDocumentStatus status;status.begin(s,project);
        const auto before=s;juce::String error;
        TrackRenamePlan pending(s,project,1);require(json(s)==json(before),"preparing dialog changed project");
        require(!pending.commit(s,project,false,"Keys",error) && json(s)==json(before),"busy rename accepted");
        const juce::String invalid[]={"","   ",juce::String::repeatedString("x",65),"Line\nBreak","Tab\tName",
            juce::String::charToString(127),juce::String::charToString(0x85),"Line"+juce::String::charToString(0x2028)+"Break"};
        for(const auto& name:invalid)require(!pending.commit(s,project,true,name,error) && error.isNotEmpty() && json(s)==json(before),"invalid name mutated project");
        auto changed=s;changed.tracks[1].notes[0].velocity=40;const auto changedJson=json(changed);
        require(!pending.commit(changed,project,true,"Keys",error) && json(changed)==changedJson,"stale notes overwritten");
        require(!pending.commit(s,root.getChildFile("Other.idwproj"),true,"Keys",error),"changed document accepted");
        TrackRenamePlan missing(s,project,9);require(!missing.commit(s,project,true,"Keys",error),"invalid track accepted");
        const auto name=juce::String::fromUTF8("  Keys \xce\xb2 \xf0\x9f\x8e\xb9  ");
        require(pending.commit(s,project,true,name,error),"Unicode rename failed");
        require(s.tracks[1].name==name.trim().toStdString() && status.hasUnsavedChanges(s),"rename or dirty status wrong");
        auto expected=before;expected.tracks[1].name=name.trim().toStdString();
        require(json(s)==json(expected),"rename changed notes audio routing instruments or mix");
        require(!pending.commit(s,project,true,"Other",error),"reused dialog accepted");
        TrackRenamePlan noOp(s,project,1);require(noOp.commit(s,project,true,name.trim(),error) && json(s)==json(expected),"no-op rename");
        TrackRenamePlan maximum(s,project,0);require(maximum.commit(s,project,true,juce::String::repeatedString("x",64),error),"64 character name rejected");
        TrackRenamePlan duplicate(s,project,0);require(duplicate.commit(s,project,true,juce::String(s.tracks[1].name),error),"duplicate display name rejected");
        const auto copy=root.getChildFile("Renamed.idwproj");require(saveProjectSession(s,copy),"save renamed");
        Session loaded;require(loadSession(copy,loaded) && loaded.tracks[1].name==s.tracks[1].name && loaded.tracks[1].notes[0].velocity==70,"rename persistence");
        // Renaming must preserve both note undo and redo stacks, including older
        // edits and entries for another track. The replacement identity guard stays.
        auto historySession=before;historySession.addTrack(TrackType::midi);historySession.addMidiNote(2,{67,0,1,90});
        MidiEditHistory history;
        auto notes=historySession.tracks[1].notes;historySession.tracks[1].notes[0].velocity=40;require(history.record(historySession,1,notes),"first history");
        notes=historySession.tracks[1].notes;historySession.tracks[1].notes[0].velocity=50;require(history.record(historySession,1,notes),"second history");
        require(history.undo(historySession)==MidiEditHistory::Result::applied,"prepare redo");
        const auto oldName=historySession.tracks[1].name;TrackRenamePlan renameHistory(historySession,project,1);
        require(renameHistory.commit(historySession,project,true,"Piano",error),"history rename");history.trackRenamed(1,oldName,"Piano");
        require(history.redo(historySession)==MidiEditHistory::Result::applied && historySession.tracks[1].notes[0].velocity==50,"redo lost after rename");
        require(history.undo(historySession)==MidiEditHistory::Result::applied && history.undo(historySession)==MidiEditHistory::Result::applied &&
            historySession.tracks[1].notes[0].velocity==70 && historySession.tracks[1].name=="Piano","older undo lost or name undone");
        require(history.redo(historySession)==MidiEditHistory::Result::applied,"restore history for conflict");
        notes=historySession.tracks[2].notes;historySession.tracks[2].notes[0].velocity=20;history.record(historySession,2,notes);
        const auto thirdName=historySession.tracks[2].name;historySession.tracks[2].name="Bass";history.trackRenamed(2,thirdName,"Bass");
        require(history.undo(historySession)==MidiEditHistory::Result::applied && historySession.tracks[2].notes[0].velocity==90,"other track history lost");
        historySession.tracks[1].name="Unrelated replacement";
        require(history.undo(historySession)==MidiEditHistory::Result::conflict,"replacement identity guard removed");
        // Clip history restores only clips and leaves explicit names intact.
        auto audioSession=before;ClipEditHistory clips;const auto clipBefore=audioSession;
        audioSession.tracks[0].clips[0].startBeat=2;require(clips.record(clipBefore,audioSession),"clip history");
        TrackRenamePlan audioRename(audioSession,project,0);require(audioRename.commit(audioSession,project,true,"Vocal",error),"audio rename");
        require(clips.undo(audioSession)==ClipEditHistory::Result::applied && audioSession.tracks[0].name=="Vocal" &&
            clips.redo(audioSession)==ClipEditHistory::Result::applied && audioSession.tracks[0].name=="Vocal","clip history changed name");
        require(project.loadFileAsString()==saved && audio.loadFileAsString()=="preserve source bytes","original files modified");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"Track rename tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
