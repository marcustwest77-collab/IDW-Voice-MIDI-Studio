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
    for(int i=0;i<editor.getNumChildComponents();++i)
        if(auto* found=dynamic_cast<juce::Slider*>(editor.getChildComponent(i)))slider=found;
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
    std::cout<<"PASS velocity inspector callbacks, grouped undo/redo, pending final value, busy/conflict/reset guards, save/reopen, MIDI and WAV dynamics\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
