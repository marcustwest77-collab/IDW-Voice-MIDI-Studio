#include "PianoRoll.h"
#include "SessionPersistence.h"
#include "OfflineBounce.h"
#include "StandardMidiExport.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& e,int x,int y){
    const auto now=juce::Time::getCurrentTime();y+=PianoRollViewport::headerHeight;
    return {juce::Desktop::getInstance().getMainMouseSource(),{(float)x,(float)y},
        juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&e,&e,now,{(float)x,(float)y},now,1,false};
}
static void select(IDWPianoRoll& e){e.mouseDown(mouse(e,55,312));e.mouseUp(mouse(e,55,312));}
static float sample(const juce::File& f){
    juce::WavAudioFormat wav;auto r=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(f.createInputStream().release(),true));
    require(r && r->lengthInSamples==12000,"WAV metadata");juce::AudioBuffer<float> b(2,1);
    require(r->read(&b,0,1,1000,true,true),"WAV sample");return b.getSample(0,0);
}
int main(int argc,char** argv){try{
    juce::ScopedJuceInitialiser_GUI init;
    Session s;s.bpm=60;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);
    require(s.addMidiNote(0,{60,0,0.25,100}),"note fixture");
    IDWPianoRoll editor;editor.session=&s;editor.setSize(1000,480);
    bool allowed=true;int conflicts=0,changes=0;
    editor.canEdit=[&]{return allowed;};editor.onHistoryConflict=[&]{++conflicts;};editor.onChanged=[&]{++changes;};
    juce::Slider* slider=nullptr;
    juce::TextButton* undoButton=nullptr;juce::TextButton* redoButton=nullptr;
    for(int i=0;i<editor.getNumChildComponents();++i)
        if(auto* found=dynamic_cast<juce::Slider*>(editor.getChildComponent(i)))slider=found;
        else if(auto* button=dynamic_cast<juce::TextButton*>(editor.getChildComponent(i))){
            if(button->getButtonText()=="UNDO MIDI")undoButton=button;
            if(button->getButtonText()=="REDO MIDI")redoButton=button;
        }
    require(undoButton && redoButton && !undoButton->isEnabled() && !redoButton->isEnabled(),"initial history buttons");
    require(slider && !slider->isEnabled() && !slider->getWantsKeyboardFocus(),"unselected control or slider focus policy");
    select(editor);require(slider->isEnabled() && slider->getValue()==100,"selected velocity not displayed");
    // Exercise the production slider callbacks, including a queued final value.
    slider->onDragStart();slider->setValue(80,juce::sendNotificationSync);
    slider->setValue(60,juce::sendNotificationSync);
    require(!editor.restoreHistory(true),"undo allowed during velocity drag");
    slider->setValue(40,juce::sendNotificationAsync);slider->onDragEnd();
    require(s.tracks[0].notes[0].velocity==40,"final pending slider value not committed");
    require(slider->getY()>=480-IDWPianoRoll::velocityFooterHeight,"velocity control overlaps note grid");
    if(argc>1){
        juce::Image preview(juce::Image::ARGB,1000,480,true,juce::SoftwareImageType{});
        {juce::Graphics graphics(preview);editor.paintEntireComponent(graphics,true);}
        const juce::File file=juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(argv[1]));
        auto stream=file.createOutputStream();require(stream!=nullptr,"preview stream");
        juce::PNGImageFormat png;require(png.writeImageToStream(preview,*stream),"preview render");
    }
    s.bpm=93;s.masterMix.gain=0.7;s.tracks[0].gain=0.5f;
    require(s.addClip(1,{"preserved.wav",0,1,0}),"new audio work");
    require(editor.restoreHistory(true) && s.tracks[0].notes[0].velocity==100,"velocity drag not one undo");
    require(s.bpm==93 && s.masterMix.gain==0.7 && s.tracks[0].gain==0.5f && s.tracks[1].clips.size()==1,"velocity undo changed unrelated work");
    require(editor.restoreHistory(false) && s.tracks[0].notes[0].velocity==40,"velocity redo");
    select(editor);slider->onDragStart();slider->onDragEnd();
    require(editor.restoreHistory(true) && s.tracks[0].notes[0].velocity==100,"no-op velocity drag entered history");
    select(editor);require(!editor.setSelectionVelocity(100),"no-op velocity accepted");
    require(editor.restoreHistory(false) && s.tracks[0].notes[0].velocity==40,"no-op velocity cleared redo");
    select(editor);allowed=false;slider->setValue(50,juce::sendNotificationSync);
    require(s.tracks[0].notes[0].velocity==40,"busy inspector edited notes");allowed=true;
    select(editor);require(!editor.setSelectionVelocity(0) && !editor.setSelectionVelocity(128),"invalid velocity accepted");
    slider->onDragStart();slider->setValue(30,juce::sendNotificationSync);
    s.tracks[0].notes.push_back({64,1,1,90});const auto external=s.tracks[0].notes;
    slider->setValue(20,juce::sendNotificationSync);slider->onDragEnd();
    require(sameMidiNotes(s.tracks[0].notes,external) && conflicts==1,"stale velocity selection overwrote newer notes");
    editor.resetEditor();require(!slider->isEnabled() && !editor.setSelectionVelocity(1) && !editor.restoreHistory(true),"project reset retained inspector/history");
    require(!s.setMidiNoteVelocity(99,0,1) && !s.setMidiNoteVelocity(1,0,1) &&
        !s.setMidiNoteVelocity(0,99,1) && !s.setMidiNoteVelocity(0,0,0),"model velocity bounds");
    // Fresh note-only project: exported and recalled velocity controls dynamics.
    s=Session{};s.bpm=60;s.addTrack(TrackType::midi);s.addMidiNote(0,{60,0,0.25,100});
    editor.resetEditor();select(editor);require(editor.setSelectionVelocity(1),"minimum velocity edit");
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("idw-midi-velocity","",false);
    require(root.createDirectory().wasOk(),"fixture directory");const auto project=root.getChildFile("Session.idwproj");
    require(saveSession(s,project),"velocity project save");Session reopened;
    require(loadSession(project,reopened) && reopened.tracks[0].notes[0].velocity==1,"velocity project reopen");
    juce::String error;const auto midi=root.getChildFile("velocity.mid");require(exportSessionMidi(reopened,midi,error),"velocity MIDI export");
    juce::MidiFile file;{juce::FileInputStream input(midi);require(file.readFrom(input,false),"velocity MIDI read");}
    bool found=false;for(int i=0;i<file.getTrack(1)->getNumEvents();++i){const auto& m=file.getTrack(1)->getEventPointer(i)->message;
        if(m.isNoteOn()){require(m.getVelocity()==1,"MIDI velocity changed");found=true;}}
    require(found,"MIDI note missing");
    const auto quiet=root.getChildFile("quiet.wav"),loud=root.getChildFile("loud.wav");
    require(renderProjectWav(reopened,quiet,error,48000),"quiet WAV");
    select(editor);require(editor.setSelectionVelocity(127),"maximum velocity edit");
    require(renderProjectWav(s,loud,error,48000),"loud WAV");
    require(std::abs(sample(quiet)*127-sample(loud))<0.00002f && std::abs(sample(loud))>0.01f,"velocity not reflected in WAV amplitude");
    require(editor.restoreHistory(true) && s.tracks[0].notes[0].velocity==1,"maximum velocity undo");
    require(editor.restoreHistory(false) && s.tracks[0].notes[0].velocity==127,"maximum velocity redo");
    require(root.deleteRecursively(),"fixture cleanup");require(changes>=6,"velocity changes not reported");
    // Use JUCE's real numeric text-entry path, not hand-invoked drag callbacks.
    s=Session{};s.addTrack(TrackType::midi);s.addMidiNote(0,{60,0,1,110});
    editor.resetEditor();select(editor);
    juce::Label* number=nullptr;
    for(int i=0;i<slider->getNumChildComponents();++i)
        if(auto* label=dynamic_cast<juce::Label*>(slider->getChildComponent(i)))number=label;
    require(number!=nullptr,"numeric label missing");
    slider->showTextBox();auto* entry=number->getCurrentTextEditor();
    require(entry!=nullptr,"numeric entry not created");
    entry->setText("40",false);slider->hideTextBox(false);
    require(s.tracks[0].notes[0].velocity==40 && slider->getValue()==40,"numeric entry did not commit");
    require(editor.restoreHistory(true) && s.tracks[0].notes[0].velocity==110,"numeric entry undo missing");
    require(editor.restoreHistory(false) && s.tracks[0].notes[0].velocity==40,"numeric entry redo missing");
    require(undoButton->isEnabled() && !redoButton->isEnabled(),"button history availability after numeric edit");
    undoButton->onClick();
    require(s.tracks[0].notes[0].velocity==110 && !undoButton->isEnabled() && redoButton->isEnabled(),"numeric edit button undo");
    redoButton->onClick();
    require(s.tracks[0].notes[0].velocity==40 && undoButton->isEnabled() && !redoButton->isEnabled(),"numeric edit button redo");
    // Button operation must be safe even when a number edit has not yet committed.
    select(editor);slider->showTextBox();entry=number->getCurrentTextEditor();
    require(entry!=nullptr,"pending numeric editor missing");entry->setText("70",false);
    undoButton->onClick();require(s.tracks[0].notes[0].velocity==40,"button lost or failed to undo pending numeric entry");
    redoButton->onClick();require(s.tracks[0].notes[0].velocity==70,"pending numeric edit redo");
    select(editor);allowed=false;undoButton->onClick();
    require(s.tracks[0].notes[0].velocity==70,"busy history button edited notes");allowed=true;
    slider->onDragStart();undoButton->onClick();redoButton->onClick();
    require(s.tracks[0].notes[0].velocity==70 && !undoButton->isEnabled() && !redoButton->isEnabled(),"history buttons edited during gesture");slider->onDragEnd();
    const auto externalVelocity=12;s.tracks[0].notes[0].velocity=externalVelocity;
    const int previousConflicts=conflicts;undoButton->onClick();
    require(s.tracks[0].notes[0].velocity==externalVelocity && conflicts==previousConflicts+1,"history button overwrote conflicting notes");
    editor.resetEditor();require(!undoButton->isEnabled() && !redoButton->isEnabled(),"reset retained history buttons");
    for(int width:{100,339,340,480,1000}){
        editor.setSize(width,480);
        require(undoButton->isVisible()==(width>=340) && redoButton->isVisible()==(width>=340),"history controls visibility at narrow widths");
        if(width>=340)require(undoButton->getX()>=0 && undoButton->getRight()<redoButton->getX() &&
            redoButton->getRight()<width-114 && undoButton->getBottom()<=PianoRollViewport::headerHeight,"history header overlap");
    }
    std::cout<<"PASS velocity inspector callbacks, numeric entry, grouped undo/redo, pending final value, busy/conflict/reset guards, save/reopen, MIDI and WAV dynamics\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
