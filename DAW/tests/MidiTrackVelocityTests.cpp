#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static juce::MouseEvent mouse(IDWPianoRoll& editor){const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{62.0f,342.0f},juce::ModifierKeys::leftButtonModifier,
        1,0,0,0,0,&editor,&editor,now,{62.0f,342.0f},now,1,false};}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-track-velocity","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        const std::vector<MidiNote> notes={{60,0.49,1.25,30},{64,2.51,0.75,90}};
        std::vector<MidiNote> output;
        for(int offset:{10,-10,20,-20}){
            require(adjustMidiTrackVelocity(notes,MidiTrackVelocityMode::offset,offset,output)==MidiTrackVelocityResult::changed,"relative adjustment");
            for(size_t i=0;i<notes.size();++i)require(output[i].velocity==notes[i].velocity+offset && output[i].pitch==notes[i].pitch &&
                output[i].startBeat==notes[i].startBeat && output[i].lengthBeats==notes[i].lengthBeats,"relative changed other fields");
            std::vector<MidiNote> back;require(adjustMidiTrackVelocity(output,MidiTrackVelocityMode::offset,-offset,back)==MidiTrackVelocityResult::changed && sameMidiNotes(back,notes),"inverse adjustment");
        }
        for(int value:{1,64,100,127}){
            require(adjustMidiTrackVelocity(notes,MidiTrackVelocityMode::fixed,value,output)==MidiTrackVelocityResult::changed,"fixed adjustment");
            auto expected=notes;for(auto& note:expected)note.velocity=value;
            require(sameMidiNotes(output,expected),"fixed changed other fields");
            require(adjustMidiTrackVelocity(output,MidiTrackVelocityMode::fixed,value,output)==MidiTrackVelocityResult::unchanged,"fixed no-op alias");
        }
        std::vector<MidiNote> canary={{90,0,1,1}};
        require(adjustMidiTrackVelocity(notes,MidiTrackVelocityMode::offset,0,canary)==MidiTrackVelocityResult::unchanged && canary[0].pitch==90,"zero changed output");
        require(adjustMidiTrackVelocity({},MidiTrackVelocityMode::fixed,64,canary)==MidiTrackVelocityResult::unchanged,"empty part");
        for(int value:{-127,127,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})
            require(adjustMidiTrackVelocity(notes,MidiTrackVelocityMode::offset,value,canary)==MidiTrackVelocityResult::invalid && canary[0].pitch==90,"invalid offset changed output");
        for(int value:{-1,0,128,std::numeric_limits<int>::max()})
            require(adjustMidiTrackVelocity(notes,MidiTrackVelocityMode::fixed,value,canary)==MidiTrackVelocityResult::invalid,"invalid fixed value");
        require(adjustMidiTrackVelocity(notes,static_cast<MidiTrackVelocityMode>(99),64,canary)==MidiTrackVelocityResult::invalid,"invalid mode");
        require(adjustMidiTrackVelocity({{60,0,1,1}},MidiTrackVelocityMode::offset,126,output)==MidiTrackVelocityResult::changed && output[0].velocity==127,"upper endpoint");
        require(adjustMidiTrackVelocity(output,MidiTrackVelocityMode::offset,-126,output)==MidiTrackVelocityResult::changed && output[0].velocity==1,"lower endpoint alias");
        for(auto mode:{MidiTrackVelocityMode::offset,MidiTrackVelocityMode::fixed})
            for(const auto& bad:std::vector<MidiNote>{{-1,0,1,70},{128,0,1,70},{60,0,1,0},{60,0,1,128},
                {60,-1,1,70},{60,0,0,70},{60,0,-1,70},{60,std::numeric_limits<double>::quiet_NaN(),1,70},
                {60,0,std::numeric_limits<double>::infinity(),70},{60,1e308,1e308,70},{60,1e20,1,70}}){
                auto mixed=notes;mixed.push_back(bad);
                require(adjustMidiTrackVelocity(mixed,mode,10,canary)==MidiTrackVelocityResult::invalid && canary[0].pitch==90,"partial invalid edit");
            }
        require(adjustMidiTrackVelocity({{60,0,1,30},{60,2,1,127}},MidiTrackVelocityMode::offset,10,canary)==MidiTrackVelocityResult::invalid && canary[0].pitch==90,"overflow partially applied/clamped");
        require(adjustMidiTrackVelocity({{60,0,1,30},{60,2,1,1}},MidiTrackVelocityMode::offset,-10,canary)==MidiTrackVelocityResult::invalid,"underflow became note-off");
        auto large=std::vector<MidiNote>(100000,{60,0,1,70});
        require(adjustMidiTrackVelocity(large,MidiTrackVelocityMode::fixed,64,output)==MidiTrackVelocityResult::changed && output.size()==100000 && output.back().velocity==64,"large part");
        large.push_back({60,0,1,70});require(adjustMidiTrackVelocity(large,MidiTrackVelocityMode::fixed,64,canary)==MidiTrackVelocityResult::invalid,"note cap");
        Session session;session.addTrack(TrackType::midi);session.addTrack(TrackType::audio);session.tracks[0].notes=notes;
        session.addClip(1,{"untouched.wav",1,4,0.25});session.bpm=93;session.masterMix.gain=0.7;session.tracks[0].synthPreset=4;
        const auto original=juce::JSON::toString(sessionToJson(session),true);
        IDWPianoRoll editor;editor.session=&session;editor.setSize(1200,480);editor.setTrackIndex(0);
        bool allowed=true;editor.canEdit=[&]{return allowed;};int changes=0;editor.onChanged=[&]{++changes;};
        juce::TextButton* action=nullptr;juce::TextButton* undo=nullptr;juce::TextButton* redo=nullptr;juce::TextButton* close=nullptr;
        juce::Slider* velocity=nullptr;juce::Label* feedback=nullptr;
        for(int i=0;i<editor.getNumChildComponents();++i){auto* child=editor.getChildComponent(i);
            if(auto* b=dynamic_cast<juce::TextButton*>(child)){
                if(b->getButtonText()=="VELOCITY TRACK")action=b;if(b->getButtonText()=="UNDO MIDI")undo=b;
                if(b->getButtonText()=="REDO MIDI")redo=b;if(b->getButtonText()=="BACK TO TRACKS")close=b;
            }
            if(auto* slider=dynamic_cast<juce::Slider*>(child))velocity=slider;
            if(child->getComponentID()=="midi-edit-feedback")feedback=dynamic_cast<juce::Label*>(child);
        }
        require(action && undo && redo && close && velocity && feedback && action->isEnabled(),"track velocity controls missing");
        allowed=false;require(!editor.adjustTrackVelocity(MidiTrackVelocityMode::offset,10) && sameMidiNotes(session.tracks[0].notes,notes),"busy edit");allowed=true;
        require(editor.adjustTrackVelocity(MidiTrackVelocityMode::offset,10) && changes==1 && session.tracks[0].notes[0].velocity==40 && session.tracks[0].notes[1].velocity==100,"editor relative edit");
        require(feedback->getText().contains("Track velocity +10 applied"),"relative feedback");
        auto unrelated=session;unrelated.tracks[0].notes=notes;require(juce::JSON::toString(sessionToJson(unrelated),true)==original,"other session data changed");
        undo->onClick();require(sameMidiNotes(session.tracks[0].notes,notes),"one button undo");
        require(!editor.adjustTrackVelocity(MidiTrackVelocityMode::offset,0) && !editor.adjustTrackVelocity(MidiTrackVelocityMode::offset,100),"no-op/invalid accepted");
        redo->onClick();require(session.tracks[0].notes[0].velocity==40 && session.tracks[0].notes[1].velocity==100,"redo lost after rejection");
        require(editor.adjustTrackVelocity(MidiTrackVelocityMode::fixed,64) && feedback->getText().contains("Track velocity set 64 applied"),"fixed editor edit");
        undo->onClick();require(session.tracks[0].notes[0].velocity==40 && session.tracks[0].notes[1].velocity==100,"fixed grouped undo");
        redo->onClick();require(session.tracks[0].notes[0].velocity==64 && session.tracks[0].notes[1].velocity==64,"fixed grouped redo");
        editor.mouseDown(mouse(editor));require(!editor.adjustTrackVelocity(MidiTrackVelocityMode::fixed,100),"edit during note drag");editor.mouseUp(mouse(editor));
        require(velocity->isEnabled(),"select note for gesture");velocity->onDragStart();
        require(!editor.adjustTrackVelocity(MidiTrackVelocityMode::fixed,100),"edit during velocity drag");velocity->onDragEnd();
        const auto adjusted=session.tracks[0].notes;close->onClick();require(root.createDirectory().wasOk(),"fixture folder");
        const auto project=root.getChildFile("Velocity.idwproj");require(saveSession(session,project),"save adjusted part");
        Session read;require(loadSession(project,read) && sameMidiNotes(read.tracks[0].notes,adjusted),"saved velocities changed");
        editor.setSize(700,480);require(!action->isVisible(),"narrow overlap");editor.setSize(1200,480);
        require(action->isVisible() && action->getRight()<=feedback->getX() && feedback->getRight()<=close->getX(),"wide header overlap");
        editor.setTrackIndex(1);require(!action->isEnabled() && !editor.adjustTrackVelocity(MidiTrackVelocityMode::fixed,64),"audio edit");
        editor.setTrackIndex(999);require(!editor.adjustTrackVelocity(MidiTrackVelocityMode::fixed,64),"invalid track edit");
        editor.session=nullptr;require(!editor.adjustTrackVelocity(MidiTrackVelocityMode::fixed,64),"null edit");
        editor.session=&session;editor.setTrackIndex(0);editor.resetEditor();require(!editor.restoreHistory(true),"reset retained history");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"MIDI track velocity tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
