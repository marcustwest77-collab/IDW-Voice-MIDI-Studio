#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& e){
    const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{71.0f,336.0f},
        juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&e,&e,now,{71.0f,336.0f},now,1,false};
}
int main(){try{
    juce::ScopedJuceInitialiser_GUI init;
    Session s;s.addTrack(TrackType::audio);s.addTrack(TrackType::midi);
    s.addMidiNote(1,{60,1,1,110});
    IDWPianoRoll editor;editor.session=&s;editor.setSize(1000,480);editor.setVisible(false);
    const auto original=juce::JSON::toString(sessionToJson(s));
    require(!editor.toggleForTrack(0) && !editor.isVisible(),"audio track opened piano roll");
    require(!editor.toggleForTrack(999) && !editor.isVisible(),"invalid track opened piano roll");
    require(editor.toggleForTrack(1) && editor.isVisible() && editor.trackIndex==1,"MIDI track did not open");
    require(editor.toggleForTrack(0) && !editor.isVisible() && editor.trackIndex==1,"audio selection trapped visible editor");
    require(editor.toggleForTrack(1) && editor.toggleForTrack(std::numeric_limits<size_t>::max()) && !editor.isVisible(),"invalid selection trapped visible editor");
    require(juce::JSON::toString(sessionToJson(s))==original,"view toggle changed project");
    juce::TextButton* back=nullptr;juce::Slider* velocity=nullptr;
    for(int i=0;i<editor.getNumChildComponents();++i){
        if(auto* b=dynamic_cast<juce::TextButton*>(editor.getChildComponent(i)))
            if(b->getButtonText()=="BACK TO TRACKS")back=b;
        if(auto* v=dynamic_cast<juce::Slider*>(editor.getChildComponent(i)))velocity=v;
    }
    require(back && velocity,"editor controls missing");
    require(editor.toggleForTrack(1),"reopen for close button");back->onClick();
    require(!editor.isVisible() && juce::JSON::toString(sessionToJson(s))==original,"back button changed project or failed to close");
    for(int width:{100,480,1000}){
        editor.setSize(width,480);
        require(back->isVisible() && back->getX()>=0 && back->getRight()<=width && back->getY()>=0 &&
            back->getBottom()<=PianoRollViewport::headerHeight,"close control outside header");
    }
    require(editor.toggleForTrack(1),"reopen for gesture");
    editor.mouseDown(mouse(editor));editor.mouseUp(mouse(editor));
    require(velocity->isEnabled() && velocity->getValue()==110,"gesture note selection");
    velocity->onDragStart();velocity->setValue(40,juce::sendNotificationAsync);
    require(editor.toggleForTrack(0) && !editor.isVisible() && s.tracks[1].notes[0].velocity==40,"close lost pending final velocity");
    require(editor.restoreHistory(true) && s.tracks[1].notes[0].velocity==110,"close did not finalize grouped undo");
    require(editor.restoreHistory(false) && s.tracks[1].notes[0].velocity==40,"close discarded redo");
    require(editor.toggleForTrack(1),"reopen before project change");
    Session replacement;replacement.addTrack(TrackType::audio);s=replacement;
    const auto replaced=juce::JSON::toString(sessionToJson(s));editor.resetEditor();
    require(!editor.isVisible() && !velocity->isEnabled() && !editor.restoreHistory(true) &&
        juce::JSON::toString(sessionToJson(s))==replaced,"project reset retained editor or old edit state");
    editor.setVisible(true);editor.session=nullptr;
    require(editor.toggleForTrack(0) && !editor.isVisible() && !editor.toggleForTrack(0),"missing session trapped editor or allowed opening");
    std::cout<<"PASS piano roll closes with audio/invalid selection, explicit back button, pending gesture undo/redo, bounded header and project reset; no audio drivers\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
