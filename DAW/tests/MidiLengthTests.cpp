#include "PianoRoll.h"
#include "SessionPersistence.h"
#include "OfflineBounce.h"
#include "StandardMidiExport.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& e,int x=71,int y=312){
    y+=PianoRollViewport::headerHeight;const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{(float)x,(float)y},
        juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&e,&e,now,{(float)x,(float)y},now,1,false};
}
static void select(IDWPianoRoll& e){e.mouseDown(mouse(e));e.mouseUp(mouse(e));}
static juce::AudioBuffer<float> audio(const juce::File& file,int frames){
    juce::WavAudioFormat wav;auto r=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(file.createInputStream().release(),true));
    require(r && r->lengthInSamples==frames && r->sampleRate==48000,"WAV duration");
    juce::AudioBuffer<float> b(2,frames);require(r->read(&b,0,frames,0,true,true),"WAV samples");return b;
}
int main(int argc,char** argv){try{
    juce::ScopedJuceInitialiser_GUI init;
    Session s;s.bpm=93;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);s.tracks[1].mute=true;
    s.addMidiNote(0,{60,1,1,87});
    IDWPianoRoll editor;editor.session=&s;editor.setSize(1000,480);
    bool allowed=true;int conflicts=0;editor.canEdit=[&]{return allowed;};editor.onHistoryConflict=[&]{++conflicts;};
    juce::TextButton* half=nullptr;juce::TextButton* twice=nullptr;juce::Slider* velocity=nullptr;
    for(int i=0;i<editor.getNumChildComponents();++i){
        if(auto* b=dynamic_cast<juce::TextButton*>(editor.getChildComponent(i))){
            if(b->getButtonText()=="HALF LENGTH")half=b;if(b->getButtonText()=="2x LENGTH")twice=b;
        }
        if(auto* slider=dynamic_cast<juce::Slider*>(editor.getChildComponent(i)))velocity=slider;
    }
    require(half && twice && velocity && !half->isEnabled() && !twice->isEnabled(),"duration controls without selection");
    require(!editor.scaleSelectionLength(2),"unselected duration edit");select(editor);
    require(half->isEnabled() && twice->isEnabled(),"duration controls disabled");
    half->onClick();require(s.tracks[0].notes[0].lengthBeats==0.5,"half duration");
    twice->onClick();twice->onClick();require(s.tracks[0].notes[0].lengthBeats==2,"double duration/repeated selection");
    require(s.tracks[0].notes[0].startBeat==1 && s.tracks[0].notes[0].pitch==60 && s.tracks[0].notes[0].velocity==87,"duration edit changed other note fields");
    s.bpm=120;s.masterMix.gain=0.8;s.tracks[0].gain=0.6f;s.addClip(1,{"preserved.wav",0,1,0});
    require(editor.restoreHistory(true) && s.tracks[0].notes[0].lengthBeats==1,"duration undo");select(editor);
    require(!editor.scaleSelectionLength(1),"no-op duration edit");
    require(editor.restoreHistory(false) && s.tracks[0].notes[0].lengthBeats==2,"no-op cleared duration redo");
    require(s.bpm==120 && s.masterMix.gain==0.8 && s.tracks[0].gain==0.6f && s.tracks[1].clips.size()==1,"duration undo changed unrelated work");
    select(editor);const auto before=s.tracks[0].notes;
    allowed=false;half->onClick();require(sameMidiNotes(before,s.tracks[0].notes),"busy duration edit");allowed=true;
    require(!editor.keyPressed(juce::KeyPress('[',0,0)) && sameMidiNotes(before,s.tracks[0].notes),"unfocused bracket edited note");
    editor.mouseDown(mouse(editor));require(!editor.scaleSelectionLength(0.5),"length edit during note drag");editor.mouseUp(mouse(editor));
    velocity->onDragStart();require(!editor.scaleSelectionLength(0.5),"length edit during velocity drag");velocity->onDragEnd();
    s.tracks[0].notes[0].velocity=1;const auto external=s.tracks[0].notes;
    require(!editor.scaleSelectionLength(0.5) && sameMidiNotes(external,s.tracks[0].notes) && conflicts==1,"stale duration edit");
    editor.resetEditor();require(!half->isEnabled() && !twice->isEnabled() && !editor.restoreHistory(true),"reset retained duration state");
    editor.setSize(700,480);require(!half->isVisible() && !twice->isVisible(),"narrow duration controls overlap");
    editor.setSize(1000,480);require(half->isVisible() && twice->isVisible() && velocity->getRight()<half->getX() && half->getRight()<twice->getX(),"footer layout overlap");
    Session invalid;invalid.addTrack(TrackType::midi);invalid.addTrack(TrackType::audio);
    invalid.tracks[0].notes={{60,1,1,87}};
    for(double factor:{0.0,-1.0,1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        require(!invalid.scaleMidiNoteLength(0,0,factor) && invalid.tracks[0].notes[0].lengthBeats==1,"invalid factor changed duration");
    require(!invalid.scaleMidiNoteLength(99,0,2) && !invalid.scaleMidiNoteLength(1,0,2) && !invalid.scaleMidiNoteLength(0,99,2),"invalid duration target");
    for(const auto& n:std::vector<MidiNote>{{60,1,1e308,87},{60,1e300,1,87},{60,0,0,87},{128,0,1,87},{60,0,1,0},
            {60,0,std::numeric_limits<double>::infinity(),87}}){
        invalid.tracks[0].notes={n};require(!invalid.scaleMidiNoteLength(0,0,2) && invalid.tracks[0].notes[0].lengthBeats==n.lengthBeats,"invalid note duration modified");}
    invalid.tracks[0].notes={{60,0,std::numeric_limits<double>::denorm_min(),87}};
    require(!invalid.scaleMidiNoteLength(0,0,0.5) && invalid.tracks[0].notes[0].lengthBeats>0,"underflow erased note duration");
    // Exact edit -> saved project -> MIDI note-off -> actual WAV endpoint.
    Session rendered;rendered.addTrack(TrackType::midi);rendered.addMidiNote(0,{60,1,1,87});
    editor.session=&rendered;editor.resetEditor();select(editor);half->onClick();
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-midi-length","",false);
    require(root.createDirectory().wasOk(),"fixture directory");juce::String error;
    const auto shortWav=root.getChildFile("short.wav");require(renderProjectWav(rendered,shortWav,error,48000),"short note WAV");
    twice->onClick();twice->onClick();const auto project=root.getChildFile("Session.idwproj");
    require(saveSession(rendered,project),"save duration edit");Session reopened;
    require(loadSession(project,reopened) && reopened.tracks[0].notes[0].lengthBeats==2,"reopen duration edit");
    const auto midi=root.getChildFile("length.mid"),longWav=root.getChildFile("long.wav");
    require(exportSessionMidi(reopened,midi,error) && renderProjectWav(reopened,longWav,error,48000),"duration export");
    juce::MidiFile file;{juce::FileInputStream input(midi);require(file.readFrom(input,false),"MIDI read");}
    bool on=false,off=false;for(int i=0;i<file.getTrack(1)->getNumEvents();++i){const auto& m=file.getTrack(1)->getEventPointer(i)->message;
        if(m.isNoteOn()){require(m.getTimeStamp()==960 && m.getNoteNumber()==60 && m.getVelocity()==87,"duration MIDI note-on");on=true;}
        if(m.isNoteOff()){require(m.getTimeStamp()==2880 && m.getNoteNumber()==60,"duration MIDI note-off");off=true;}}
    require(on && off,"missing MIDI pair");const auto shortAudio=audio(shortWav,36000),longAudio=audio(longWav,72000);
    require(longAudio.getMagnitude(0,24000)==0 && longAudio.getMagnitude(36000,34000)>0.01f,"WAV onset or extended duration");
    for(int i=1000;i<2000;++i)require(std::abs(shortAudio.getSample(0,24000+i)-longAudio.getSample(0,24000+i))<0.000001f,"duration changed pitch/velocity samples");
    require(std::abs(longAudio.getSample(0,71999))<0.000001f,"release did not follow new endpoint");
    if(argc>1){
        juce::Image image(juce::Image::ARGB,1000,480,true,juce::SoftwareImageType{});{juce::Graphics g(image);editor.paintEntireComponent(g,true);}
        auto output=juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(argv[1])).createOutputStream();
        require(output!=nullptr,"preview stream");juce::PNGImageFormat png;require(png.writeImageToStream(image,*output),"preview render");
    }
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS note duration controls, undo/redo, no-op/busy/focus/drag/stale guards, overflow/underflow, layout, save/reopen and MIDI/WAV endpoints\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
