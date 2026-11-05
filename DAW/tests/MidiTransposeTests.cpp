#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static juce::MouseEvent mouse(IDWPianoRoll& editor){const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{62.0f,186.0f},juce::ModifierKeys::leftButtonModifier,
        1,0,0,0,0,&editor,&editor,now,{62.0f,186.0f},now,1,false};}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-midi-transpose","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        const std::vector<MidiNote> notes={{60,0.49,1.25,30},{64,2.51,0.75,90}};
        std::vector<MidiNote> output;
        for(int offset:{1,-1,2,-2,12,-12}){
            require(transposeMidiNotes(notes,offset,output)==MidiTransposeResult::changed,"supported transpose");
            for(size_t i=0;i<notes.size();++i)require(output[i].pitch==notes[i].pitch+offset && output[i].startBeat==notes[i].startBeat &&
                output[i].lengthBeats==notes[i].lengthBeats && output[i].velocity==notes[i].velocity,"transpose changed timing or velocity");
            std::vector<MidiNote> back;require(transposeMidiNotes(output,-offset,back)==MidiTransposeResult::changed && sameMidiNotes(back,notes),"inverse transpose");
        }
        std::vector<MidiNote> canary={{90,0,1,1}};
        require(transposeMidiNotes(notes,0,canary)==MidiTransposeResult::unchanged && canary[0].pitch==90,"zero offset");
        require(transposeMidiNotes({},12,canary)==MidiTransposeResult::unchanged,"empty track");
        for(int offset:{-128,128,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})
            require(transposeMidiNotes(notes,offset,canary)==MidiTransposeResult::invalid && canary[0].pitch==90,"invalid offset changed output");
        require(transposeMidiNotes({{0,0,1,70}},127,output)==MidiTransposeResult::changed && output[0].pitch==127,"upper endpoint");
        require(transposeMidiNotes(output,-127,output)==MidiTransposeResult::changed && output[0].pitch==0,"lower endpoint/in-place");
        for(const auto& bad:std::vector<MidiNote>{{127,0,1,70},{-1,0,1,70},{128,0,1,70},{60,0,1,0},{60,0,1,128},
            {60,-1,1,70},{60,0,0,70},{60,0,-1,70},{60,std::numeric_limits<double>::quiet_NaN(),1,70},
            {60,0,std::numeric_limits<double>::infinity(),70},{60,1e308,1e308,70},{60,1e20,1,70}}){
            auto mixed=notes;mixed.push_back(bad);
            require(transposeMidiNotes(mixed,1,canary)==MidiTransposeResult::invalid && canary[0].pitch==90,"partial invalid transpose");
        }
        require(transposeMidiNotes({{0,0,1,70}},-1,canary)==MidiTransposeResult::invalid && canary[0].pitch==90,"pitch underflow clamped");
        auto large=std::vector<MidiNote>(100000,{60,0,1,70});
        require(transposeMidiNotes(large,12,output)==MidiTransposeResult::changed && output.size()==100000 && output.back().pitch==72,"large transpose");
        large.push_back({60,0,1,70});require(transposeMidiNotes(large,12,canary)==MidiTransposeResult::invalid,"note cap");
        Session session;session.addTrack(TrackType::midi);session.addTrack(TrackType::audio);session.tracks[0].notes=notes;
        session.addClip(1,{"untouched.wav",1,4,0.25});session.bpm=93;session.masterMix.gain=0.7;session.tracks[0].synthPreset=4;
        const auto original=juce::JSON::toString(sessionToJson(session),true);
        IDWPianoRoll editor;editor.session=&session;editor.setSize(1200,480);editor.setTrackIndex(0);
        bool allowed=true;editor.canEdit=[&]{return allowed;};int changes=0;editor.onChanged=[&]{++changes;};
        juce::TextButton* transpose=nullptr;juce::TextButton* undo=nullptr;juce::TextButton* redo=nullptr;juce::TextButton* close=nullptr;
        juce::Slider* velocity=nullptr;juce::Label* feedback=nullptr;
        for(int i=0;i<editor.getNumChildComponents();++i){auto* child=editor.getChildComponent(i);
            if(auto* b=dynamic_cast<juce::TextButton*>(child)){
                if(b->getButtonText()=="TRANSPOSE TRACK")transpose=b;if(b->getButtonText()=="UNDO MIDI")undo=b;
                if(b->getButtonText()=="REDO MIDI")redo=b;if(b->getButtonText()=="BACK TO TRACKS")close=b;
            }
            if(auto* slider=dynamic_cast<juce::Slider*>(child))velocity=slider;
            if(child->getComponentID()=="midi-edit-feedback")feedback=dynamic_cast<juce::Label*>(child);
        }
        require(transpose && undo && redo && close && velocity && feedback && transpose->isEnabled(),"transpose controls missing");
        allowed=false;require(!editor.transposeTrack(12) && sameMidiNotes(session.tracks[0].notes,notes),"busy transpose");allowed=true;
        require(editor.transposeTrack(12) && changes==1 && session.tracks[0].notes[0].pitch==72 && session.tracks[0].notes[1].pitch==76,"editor octave");
        require(feedback->getText().contains("Transpose +12 applied"),"transpose feedback");
        auto unrelated=session;unrelated.tracks[0].notes=notes;require(juce::JSON::toString(sessionToJson(unrelated),true)==original,"transpose modified other session data");
        undo->onClick();require(sameMidiNotes(session.tracks[0].notes,notes),"actual button undo");
        require(!editor.transposeTrack(0) && !editor.transposeTrack(127),"no-op/invalid request");
        redo->onClick();require(session.tracks[0].notes[0].pitch==72 && session.tracks[0].notes[1].pitch==76,"redo lost after no-op/rejection");
        const auto transposed=session.tracks[0].notes;
        editor.mouseDown(mouse(editor));require(!editor.transposeTrack(1) && sameMidiNotes(session.tracks[0].notes,transposed),"transpose during note drag");
        editor.mouseUp(mouse(editor));require(velocity->isEnabled(),"select transposed note");
        velocity->onDragStart();require(!editor.transposeTrack(1),"transpose during velocity gesture");velocity->onDragEnd();
        close->onClick();require(root.createDirectory().wasOk(),"fixture folder");
        const auto project=root.getChildFile("Transposed.idwproj");require(saveSession(session,project),"save transpose");
        Session read;require(loadSession(project,read) && sameMidiNotes(read.tracks[0].notes,transposed),"saved transpose changed pitches");
        editor.setSize(550,480);require(!transpose->isVisible(),"narrow transpose overlaps close");
        editor.setSize(1200,480);require(transpose->isVisible() && transpose->getRight()<=feedback->getX() && feedback->getRight()<=close->getX(),"header controls overlap");
        editor.setTrackIndex(1);require(!transpose->isEnabled() && !editor.transposeTrack(12),"audio track transpose");
        editor.setTrackIndex(999);require(!editor.transposeTrack(12),"invalid track transpose");editor.session=nullptr;require(!editor.transposeTrack(12),"null transpose");
        editor.session=&session;editor.setTrackIndex(0);editor.resetEditor();require(!editor.restoreHistory(true),"reset retained transpose history");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"MIDI transpose tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
