#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
static juce::MouseEvent noteMouse(IDWPianoRoll& editor){
    const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{62.0f,342.0f},juce::ModifierKeys::leftButtonModifier,
        1,0,0,0,0,&editor,&editor,now,{62.0f,342.0f},now,1,false};
}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-midi-quantize","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        std::vector<MidiNote> input={{60,0.124,0.7,30},{64,0.125,1.25,70},{67,0.375,0.1,100},{60,0.13,0.5,90}};
        const auto original=input;std::vector<MidiNote> output;
        require(quantizeMidiNotes(input,4,output)==MidiQuantizeResult::changed,"sixteenth quantize");
        require(output[0].startBeat==0 && output[1].startBeat==0.25 && output[2].startBeat==0.5 && output[3].startBeat==0.25,"rounding or half-grid tie");
        for(size_t i=0;i<input.size();++i)require(input[i].startBeat==original[i].startBeat && output[i].pitch==input[i].pitch &&
            output[i].velocity==input[i].velocity && output[i].lengthBeats==input[i].lengthBeats,"non-timing data or input changed");
        auto canary=std::vector<MidiNote>{{90,12,2,1}};
        require(quantizeMidiNotes(output,4,canary)==MidiQuantizeResult::unchanged && canary[0].pitch==90,"idempotence or no-op output");
        for(int divisions:{1,2,3,4,6,8}){
            const std::vector<MidiNote> half={{60,0.5/divisions,0.5,70}};
            require(quantizeMidiNotes(half,divisions,output)==MidiQuantizeResult::changed && output[0].startBeat==1.0/divisions,"grid tie forward");
        }
        for(int divisions:{-1,0,5,7,16,std::numeric_limits<int>::max()})
            require(quantizeMidiNotes(input,divisions,canary)==MidiQuantizeResult::invalid && canary[0].pitch==90,"unsupported grid");
        for(const auto& bad:std::vector<MidiNote>{{128,0,1,70},{-1,0,1,70},{60,0,1,0},{60,0,1,128},
            {60,-0.1,1,70},{60,0,0,70},{60,0,-1,70},{60,std::numeric_limits<double>::infinity(),1,70},
            {60,std::numeric_limits<double>::quiet_NaN(),1,70},{60,0,std::numeric_limits<double>::infinity(),70},
            {60,1e300,1e300,70},{60,1e308,1e308,70},{60,1e20,1,70}}){
            auto notes=input;notes.push_back(bad);
            require(quantizeMidiNotes(notes,4,canary)==MidiQuantizeResult::invalid && canary[0].pitch==90,"partial or invalid quantize");
        }
        require(quantizeMidiNotes({},4,canary)==MidiQuantizeResult::unchanged,"empty notes");
        auto large=std::vector<MidiNote>(100000,{60,0.13,1,70});
        require(quantizeMidiNotes(large,4,output)==MidiQuantizeResult::changed && output.size()==100000 && output.back().startBeat==0.25,"large track");
        large.push_back({60,0,1,70});require(quantizeMidiNotes(large,4,canary)==MidiQuantizeResult::invalid,"count bound");
        Session session;session.addTrack(TrackType::midi);session.addTrack(TrackType::audio);
        session.tracks[0].notes=original;session.addClip(1,{"preserved.wav",1,4,0.5});
        session.bpm=93;session.masterMix.gain=0.6;session.tracks[0].synthPreset=4;
        const auto before=juce::JSON::toString(sessionToJson(session),true);
        IDWPianoRoll editor;editor.session=&session;editor.setSize(1000,480);bool allowed=true;int changed=0;
        editor.canEdit=[&]{return allowed;};editor.onChanged=[&]{++changed;};editor.setTrackIndex(0);
        juce::TextButton* button=nullptr;juce::Slider* velocity=nullptr;
        for(int i=0;i<editor.getNumChildComponents();++i){
            if(auto* b=dynamic_cast<juce::TextButton*>(editor.getChildComponent(i)))if(b->getButtonText()=="QUANTIZE TRACK")button=b;
            if(auto* slider=dynamic_cast<juce::Slider*>(editor.getChildComponent(i)))velocity=slider;
        }
        require(button && button->isEnabled() && button->isVisible() && velocity,"quantize control missing");
        allowed=false;require(!editor.quantizeTrack(4) && juce::JSON::toString(sessionToJson(session),true)==before,"busy editor changed notes");allowed=true;
        require(!editor.quantizeTrack(5),"invalid editor grid");
        require(editor.quantizeTrack(4) && changed==1,"editor quantize");
        auto expected=session;expected.tracks[0].notes=original;
        require(juce::JSON::toString(sessionToJson(expected),true)==before,"editor changed unrelated session state");
        require(editor.restoreHistory(true) && sameMidiNotes(session.tracks[0].notes,original),"one-step undo");
        require(!editor.quantizeTrack(5) && editor.restoreHistory(false),"invalid quantize destroyed redo");
        const auto snapped=session.tracks[0].notes;
        require(!editor.quantizeTrack(4),"already aligned quantize changed state");
        require(editor.restoreHistory(true) && sameMidiNotes(session.tracks[0].notes,original),"no-op added history");
        require(editor.restoreHistory(false) && sameMidiNotes(session.tracks[0].notes,snapped),"redo mismatch");
        require(editor.quantizeTrack(1) && editor.restoreHistory(true),"prepare coarse-grid redo");
        require(!editor.quantizeTrack(4) && editor.restoreHistory(false),"aligned no-op destroyed redo");
        require(editor.restoreHistory(true) && sameMidiNotes(session.tracks[0].notes,snapped),"restore fine grid");
        editor.setSize(400,480);require(!button->isVisible(),"narrow header overlap");editor.setSize(1000,480);require(button->isVisible(),"wide control hidden");
        editor.setTrackIndex(1);require(!button->isEnabled() && !editor.quantizeTrack(4),"audio track quantized");
        editor.setTrackIndex(999);require(!editor.quantizeTrack(4),"invalid track quantized");
        editor.session=nullptr;require(!editor.quantizeTrack(4),"null session quantized");editor.session=&session;editor.setTrackIndex(0);
        editor.mouseDown(noteMouse(editor));
        require(!editor.quantizeTrack(1) && sameMidiNotes(session.tracks[0].notes,snapped),"quantize during note drag");
        editor.mouseUp(noteMouse(editor));require(velocity->isEnabled(),"note selection for velocity gesture");
        velocity->onDragStart();
        require(!editor.quantizeTrack(1) && sameMidiNotes(session.tracks[0].notes,snapped),"quantize during velocity gesture");
        velocity->onDragEnd();
        require(root.createDirectory().wasOk(),"fixture folder");
        const auto file=root.getChildFile("Quantized.idwproj");require(saveSession(session,file),"save quantized notes");
        Session reopened;require(loadSession(file,reopened) && sameMidiNotes(reopened.tracks[0].notes,snapped),"reopen quantized notes");
        editor.resetEditor();require(!editor.restoreHistory(true),"project reset retained quantize undo");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"MIDI quantize tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
