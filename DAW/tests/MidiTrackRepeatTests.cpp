#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& e){const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{78.0f,340.0f},juce::ModifierKeys::leftButtonModifier,
        1,0,0,0,0,&e,&e,now,{78.0f,340.0f},now,1,false};}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-track-repeat","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        // Unsorted, overlapping notes and a leading rest remain intact.
        const std::vector<MidiNote> notes={{64,2.5,1.5,90},{60,1.25,2,30}};
        std::vector<MidiNote> output;
        for(int plays:{2,4,8}){
            require(repeatMidiTrack(notes,plays,4,4,output)==MidiTrackRepeatResult::changed,"repeat supported count");
            require(output.size()==notes.size()*static_cast<size_t>(plays),"repeat count");
            for(int play=0;play<plays;++play)for(size_t i=0;i<notes.size();++i){auto expected=notes[i];expected.startBeat+=4*play;
                require(sameMidiNotes({output[static_cast<size_t>(play)*notes.size()+i]},{expected}),"repeat altered notes/order or added exact-bar gap");}
        }
        require(repeatMidiTrack(notes,2,3,4,output)==MidiTrackRepeatResult::changed && output[2].startBeat==8.5,"3/4 bar rounding");
        require(repeatMidiTrack(notes,2,6,8,output)==MidiTrackRepeatResult::changed && output[2].startBeat==8.5,"6/8 quarter-beat units");
        require(repeatMidiTrack({{60,0.5,0.5,70}},2,5,16,output)==MidiTrackRepeatResult::changed && output[1].startBeat==1.75,"fractional bar");
        require(repeatMidiTrack(notes,2,4,4,output)==MidiTrackRepeatResult::changed,"alias setup");
        require(repeatMidiTrack(output,2,4,4,output)==MidiTrackRepeatResult::changed && output.size()==8 && output.back().startBeat==13.25,"in-place repeat");
        std::vector<MidiNote> canary={{90,0,1,1}};
        require(repeatMidiTrack(notes,1,4,4,canary)==MidiTrackRepeatResult::unchanged && canary[0].pitch==90,"one play changed output");
        require(repeatMidiTrack({},8,4,4,canary)==MidiTrackRepeatResult::unchanged && canary[0].pitch==90,"empty changed output");
        for(int plays:{-1,0,3,9,std::numeric_limits<int>::max()})
            require(repeatMidiTrack(notes,plays,4,4,canary)==MidiTrackRepeatResult::invalid && canary[0].pitch==90,"invalid plays changed output");
        for(int meter:{-1,0,33,std::numeric_limits<int>::max()}){
            require(repeatMidiTrack(notes,2,meter,4,canary)==MidiTrackRepeatResult::invalid,"invalid numerator");
            require(repeatMidiTrack(notes,2,4,meter,canary)==MidiTrackRepeatResult::invalid,"invalid denominator");
        }
        for(const auto& bad:std::vector<MidiNote>{{-1,0,1,70},{128,0,1,70},{60,0,1,0},{60,0,1,128},
            {60,-1,1,70},{60,0,0,70},{60,0,-1,70},{60,std::numeric_limits<double>::quiet_NaN(),1,70},
            {60,0,std::numeric_limits<double>::infinity(),70},{60,1e308,1e308,70},{60,1e20,1,70}}){
            auto mixed=notes;mixed.push_back(bad);
            require(repeatMidiTrack(mixed,2,4,4,canary)==MidiTrackRepeatResult::invalid && canary[0].pitch==90,"invalid partial output");
        }
        require(repeatMidiTrack({{60,0,1e308,70}},8,4,4,canary)==MidiTrackRepeatResult::invalid && canary[0].pitch==90,"overflow partially published");
        auto large=std::vector<MidiNote>(12500,{60,0,1,70});
        require(repeatMidiTrack(large,8,4,4,output)==MidiTrackRepeatResult::changed && output.size()==100000 && output.back().startBeat==28,"note cap endpoint");
        large.push_back({60,0,1,70});require(repeatMidiTrack(large,8,4,4,canary)==MidiTrackRepeatResult::invalid,"note cap exceeded");
        Session s;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);s.tracks[0].notes=notes;
        s.bpm=93;s.masterMix.gain=0.8;s.tracks[0].synthPreset=4;s.addClip(1,{"untouched.wav",1,2,0});
        const auto original=juce::JSON::toString(sessionToJson(s),true);
        IDWPianoRoll editor;editor.session=&s;editor.setSize(1200,480);editor.setTrackIndex(0);
        bool allowed=true;int changes=0;editor.canEdit=[&]{return allowed;};editor.onChanged=[&]{++changes;};
        juce::TextButton* action=nullptr;juce::TextButton* undo=nullptr;juce::TextButton* redo=nullptr;
        juce::Slider* velocity=nullptr;juce::Label* feedback=nullptr;
        for(int i=0;i<editor.getNumChildComponents();++i){auto* child=editor.getChildComponent(i);
            if(auto* b=dynamic_cast<juce::TextButton*>(child)){
                if(b->getButtonText()=="REPEAT TRACK")action=b;if(b->getButtonText()=="UNDO MIDI")undo=b;if(b->getButtonText()=="REDO MIDI")redo=b;}
            if(auto* v=dynamic_cast<juce::Slider*>(child))velocity=v;
            if(child->getComponentID()=="midi-edit-feedback")feedback=dynamic_cast<juce::Label*>(child);
        }
        require(action && undo && redo && velocity && feedback && action->isEnabled(),"repeat controls missing");
        allowed=false;require(!editor.repeatTrack(2) && sameMidiNotes(s.tracks[0].notes,notes),"busy repeat");allowed=true;
        require(editor.repeatTrack(2) && changes==1 && s.tracks[0].notes.size()==4 && feedback->getText().contains("Repeat 2 plays applied"),"editor repeat");
        auto other=s;other.tracks[0].notes=notes;require(juce::JSON::toString(sessionToJson(other),true)==original,"unrelated session changed");
        undo->onClick();require(sameMidiNotes(s.tracks[0].notes,notes),"one button undo");
        require(!editor.repeatTrack(1) && !editor.repeatTrack(3),"no-op/invalid accepted");redo->onClick();
        require(s.tracks[0].notes.size()==4,"rejection destroyed redo");undo->onClick();
        editor.mouseDown(mouse(editor));require(!editor.repeatTrack(2),"repeat during note gesture");editor.mouseUp(mouse(editor));
        require(velocity->isEnabled(),"gesture selection");velocity->onDragStart();require(!editor.repeatTrack(2),"repeat during velocity gesture");velocity->onDragEnd();
        require(editor.repeatTrack(4) && s.tracks[0].notes.size()==8,"four plays");
        const auto repeated=s.tracks[0].notes;require(root.createDirectory().wasOk(),"fixture directory");const auto file=root.getChildFile("Repeated.idwproj");
        require(saveSession(s,file),"save repeated part");Session read;
        require(loadSession(file,read) && sameMidiNotes(read.tracks[0].notes,repeated),"saved repeat changed");
        editor.setSize(900,480);require(!action->isVisible(),"narrow repeat overlaps footer");editor.setSize(1200,480);
        require(action->isVisible() && velocity->getRight()<=action->getX(),"repeat overlaps inspector");
        editor.setTrackIndex(1);require(!action->isEnabled() && !editor.repeatTrack(2),"audio target");
        editor.setTrackIndex(999);require(!editor.repeatTrack(2),"invalid target");editor.session=nullptr;require(!editor.repeatTrack(2),"null target");
        editor.session=&s;editor.setTrackIndex(0);editor.resetEditor();require(!editor.restoreHistory(true),"reset retained repeat history");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"MIDI track repeat tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
