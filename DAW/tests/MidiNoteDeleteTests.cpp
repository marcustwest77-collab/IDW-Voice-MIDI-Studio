#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& e){const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{78.0f,340.0f},juce::ModifierKeys::leftButtonModifier,
        1,0,0,0,0,&e,&e,now,{78.0f,340.0f},now,1,false};}
static void select(IDWPianoRoll& e){e.mouseDown(mouse(e));e.mouseUp(mouse(e));}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-note-delete","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        Session s;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);s.addTrack(TrackType::midi);
        s.tracks[0].notes={{60,1.25,1,30},{64,3,2,40},{60,1.25,0.5,70}};
        s.tracks[2].notes={{72,0,1,100}};s.bpm=93;s.masterMix.gain=0.8;s.addClip(1,{"untouched.wav",1,3,0});
        const auto before=s.tracks[0].notes;const auto original=juce::JSON::toString(sessionToJson(s),true);
        IDWPianoRoll editor;editor.session=&s;editor.setSize(1200,480);editor.setTrackIndex(0);
        bool allowed=true;int changes=0,conflicts=0;editor.canEdit=[&]{return allowed;};editor.onChanged=[&]{++changes;};editor.onHistoryConflict=[&]{++conflicts;};
        juce::TextButton* action=nullptr;juce::TextButton* undo=nullptr;juce::TextButton* redo=nullptr;
        juce::Slider* velocity=nullptr;juce::Label* feedback=nullptr;
        for(int i=0;i<editor.getNumChildComponents();++i){auto* child=editor.getChildComponent(i);
            if(auto* b=dynamic_cast<juce::TextButton*>(child)){
                if(b->getButtonText()=="DELETE NOTE")action=b;if(b->getButtonText()=="UNDO MIDI")undo=b;if(b->getButtonText()=="REDO MIDI")redo=b;}
            if(auto* v=dynamic_cast<juce::Slider*>(child))velocity=v;
            if(child->getComponentID()=="midi-edit-feedback")feedback=dynamic_cast<juce::Label*>(child);
        }
        require(action && undo && redo && velocity && feedback && !action->isEnabled(),"delete controls/default");
        require(!editor.deleteSelection() && sameMidiNotes(s.tracks[0].notes,before) && changes==0,"unselected delete");
        select(editor);require(action->isEnabled() && velocity->getValue()==70,"overlap selects topmost note");
        action->onClick();require(s.tracks[0].notes.size()==2 && sameMidiNotes(s.tracks[0].notes,{before[0],before[1]}) && changes==1,"button deleted wrong notes");
        require(!action->isEnabled() && feedback->getText().contains("Delete note applied"),"delete did not clear selection/feedback");
        auto other=s;other.tracks[0].notes=before;require(juce::JSON::toString(sessionToJson(other),true)==original,"delete changed other session data");
        undo->onClick();require(sameMidiNotes(s.tracks[0].notes,before),"one undo did not restore exact index/fields");
        require(!editor.deleteSelection(),"missing selection accepted after undo");redo->onClick();
        require(s.tracks[0].notes.size()==2,"no-op delete destroyed redo");undo->onClick();
        select(editor);allowed=false;action->onClick();require(!editor.deleteSelection() && sameMidiNotes(s.tracks[0].notes,before),"busy delete");allowed=true;
        editor.mouseDown(mouse(editor));action->onClick();require(!editor.deleteSelection(),"delete during note gesture");editor.mouseUp(mouse(editor));
        velocity->onDragStart();action->onClick();require(!editor.deleteSelection(),"delete during velocity gesture");velocity->onDragEnd();
        select(editor);s.tracks[0].name="Replacement";action->onClick();require(sameMidiNotes(s.tracks[0].notes,before) && conflicts==1,"stale renamed selection deleted");
        s.tracks[0].name="MIDI 1";select(editor);s.tracks[0].notes[0].velocity=99;action->onClick();
        require(s.tracks[0].notes.size()==3 && conflicts==2,"stale note selection deleted");s.tracks[0].notes=before;
        select(editor);action->onClick();require(root.createDirectory().wasOk(),"fixture directory");const auto file=root.getChildFile("Deleted.idwproj");
        require(saveSession(s,file),"save delete");Session read;
        require(loadSession(file,read) && sameMidiNotes(read.tracks[0].notes,{before[0],before[1]}) && sameMidiNotes(read.tracks[2].notes,s.tracks[2].notes),"save/reopen delete");
        Session lone;lone.addTrack(TrackType::midi);lone.tracks[0].notes={before[0]};
        editor.resetEditor();editor.session=&lone;editor.setTrackIndex(0);select(editor);action->onClick();
        require(lone.tracks[0].notes.empty() && undo->isEnabled(),"last-note delete disabled undo");
        undo->onClick();require(sameMidiNotes(lone.tracks[0].notes,{before[0]}),"last-note undo");
        redo->onClick();require(lone.tracks[0].notes.empty(),"last-note redo");
        editor.resetEditor();editor.session=&s;editor.setTrackIndex(0);
        editor.setSize(600,480);require(!action->isVisible(),"narrow delete overlap");editor.setSize(1200,480);
        require(action->isVisible() && velocity->getRight()<=action->getX(),"wide delete overlaps inspector");
        editor.setTrackIndex(1);require(!action->isEnabled() && !editor.deleteSelection(),"audio delete");
        editor.setTrackIndex(999);require(!editor.deleteSelection(),"invalid target delete");editor.session=nullptr;require(!editor.deleteSelection(),"null delete");
        editor.session=&s;editor.setTrackIndex(0);editor.resetEditor();require(!editor.restoreHistory(true) && !action->isEnabled(),"reset retained delete state");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"MIDI note delete tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
