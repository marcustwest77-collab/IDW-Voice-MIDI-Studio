#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-note-selection","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        const std::vector<MidiNote> notes={{64,3,1,40},{60,0,1,30},{60,0,0.5,70},{0,2,1,20},{127,5,1,90}};
        const std::vector<size_t> order={1,2,3,0,4};
        require(adjacentMidiNote(notes,{},true)==1 && adjacentMidiNote(notes,{},false)==4,"initial chronological selection");
        for(size_t i=0;i<order.size();++i){
            require(adjacentMidiNote(notes,order[i],true)==order[(i+1)%order.size()],"forward order/wrap");
            require(adjacentMidiNote(notes,order[i],false)==order[(i+order.size()-1)%order.size()],"reverse order/wrap");
        }
        require(!adjacentMidiNote({}, {},true) && !adjacentMidiNote(notes,999,true),"empty/invalid selection");
        require(adjacentMidiNote({notes[0]},0,true)==0 && adjacentMidiNote({notes[0]},0,false)==0,"single note wrap");
        for(const auto& bad:std::vector<MidiNote>{{-1,0,1,70},{128,0,1,70},{60,0,1,0},{60,0,1,128},
            {60,-1,1,70},{60,0,0,70},{60,0,-1,70},{60,std::numeric_limits<double>::quiet_NaN(),1,70},
            {60,0,std::numeric_limits<double>::infinity(),70},{60,1e308,1e308,70},{60,1e20,1,70}}){
            auto mixed=notes;mixed.push_back(bad);require(!adjacentMidiNote(mixed,{},true),"invalid notes accepted");
        }
        auto large=std::vector<MidiNote>(100000,{60,0,1,70});
        require(adjacentMidiNote(large,99999,true)==0 && adjacentMidiNote(large,0,false)==99999,"large selection wrap");
        large.push_back({60,0,1,70});require(!adjacentMidiNote(large,{},true),"selection note cap");
        Session s;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);s.tracks[0].notes=notes;
        s.bpm=93;s.masterMix.gain=0.8;s.addClip(1,{"untouched.wav",1,2,0});const auto original=juce::JSON::toString(sessionToJson(s),true);
        IDWPianoRoll editor;editor.session=&s;editor.setSize(1200,480);editor.setTrackIndex(0);
        bool allowed=true;int changes=0,conflicts=0;editor.canEdit=[&]{return allowed;};editor.onChanged=[&]{++changes;};editor.onHistoryConflict=[&]{++conflicts;};
        juce::TextButton* previous=nullptr;juce::TextButton* next=nullptr;juce::Slider* velocity=nullptr;juce::Label* summary=nullptr;
        for(int i=0;i<editor.getNumChildComponents();++i){auto* child=editor.getChildComponent(i);
            if(auto* b=dynamic_cast<juce::TextButton*>(child)){if(b->getButtonText()=="NOTE <")previous=b;if(b->getButtonText()=="NOTE >")next=b;}
            if(auto* v=dynamic_cast<juce::Slider*>(child))velocity=v;
            if(child->getComponentID()=="midi-note-summary")summary=dynamic_cast<juce::Label*>(child);
        }
        require(previous && next && velocity && summary && next->isEnabled() && summary->getText()=="No note selected","selection controls/default");
        next->onClick();require(velocity->getValue()==30 && summary->getText().contains("Pitch C4 (60)") && summary->getText().contains("Start 0.000 beats") && summary->getText().contains("Length 1.000 beats") && summary->getText().contains("Velocity 30"),"first selected note summary");
        next->onClick();require(velocity->getValue()==70 && summary->getText().contains("Length 0.500 beats"),"stable duplicate order");
        next->onClick();require(summary->getText().contains("Pitch C-1 (0)"),"low pitch selection/reveal");
        next->onClick();require(velocity->getValue()==40 && summary->getText().contains("Start 3.000 beats"),"unsorted chronological selection");
        next->onClick();require(summary->getText().contains("Pitch G9 (127)"),"high pitch selection/reveal");
        next->onClick();require(velocity->getValue()==30,"forward wrap");previous->onClick();require(velocity->getValue()==90,"previous wrap");
        require(changes==0 && juce::JSON::toString(sessionToJson(s),true)==original && !editor.restoreHistory(true),"navigation edited project/history");
        require(editor.setSelectionVelocity(100) && summary->getText().contains("Velocity 100"),"summary after velocity edit");
        require(editor.restoreHistory(true),"velocity undo");next->onClick();next->onClick();
        require(editor.restoreHistory(false) && s.tracks[0].notes[4].velocity==100,"selection destroyed redo");
        require(editor.restoreHistory(true),"restore original");next->onClick();const auto shown=summary->getText();
        allowed=false;next->onClick();require(!editor.selectAdjacentNote(true) && summary->getText()==shown,"busy selection");allowed=true;
        velocity->onDragStart();require(!editor.selectAdjacentNote(false),"selection during velocity gesture");velocity->onDragEnd();
        s.tracks[0].name="Replacement";require(!editor.selectAdjacentNote(true) && conflicts==1 && summary->getText()=="No note selected","stale selection target");s.tracks[0].name="MIDI 1";
        next->onClick();require(summary->getTooltip()==summary->getText(),"full summary tooltip");
        require(root.createDirectory().wasOk(),"fixture directory");const auto file=root.getChildFile("Selected.idwproj");require(saveSession(s,file),"save selection");Session read;
        require(loadSession(file,read) && juce::JSON::toString(sessionToJson(read),true)==original,"selection serialized/mutated project");
        editor.setSize(600,480);require(!summary->isVisible(),"narrow summary overlap");editor.setSize(1200,480);
        require(summary->isVisible() && previous->getRight()<=next->getX() && next->getRight()<=summary->getX() && summary->getBottom()<=velocity->getY(),"selection row overlap");
        editor.setTrackIndex(1);require(!next->isEnabled() && !editor.selectAdjacentNote(true),"audio selection");
        editor.setTrackIndex(999);require(!editor.selectAdjacentNote(true),"invalid target selection");editor.session=nullptr;require(!editor.selectAdjacentNote(true),"null selection");
        editor.session=&s;editor.setTrackIndex(0);editor.resetEditor();require(summary->getText()=="No note selected","reset retained summary");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"MIDI note selection tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
