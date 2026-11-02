#include "PianoRoll.h"
#include "SessionPersistence.h"
#include "OfflineBounce.h"
#include "StandardMidiExport.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& editor,int x,int y){
    y+=PianoRollViewport::headerHeight;const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{(float)x,(float)y},
        juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&editor,&editor,now,{(float)x,(float)y},now,1,false};
}
static void select(IDWPianoRoll& e,int x=63){e.mouseDown(mouse(e,x,260));e.mouseUp(mouse(e,x,260));}
int main(int argc,char** argv){try{
    juce::ScopedJuceInitialiser_GUI init;
    Session s;s.bpm=93;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);
    s.masterMix.gain=0.71;s.tracks[1].mute=true;
    require(s.addMidiNote(0,{64,0.5,0.75,73}) && s.addMidiNote(0,{67,3,1,100}),"note fixtures");
    const auto original=s.tracks[0].notes;
    IDWPianoRoll editor;editor.session=&s;editor.setSize(1000,480);
    bool allowed=true;int conflicts=0;editor.canEdit=[&]{return allowed;};editor.onHistoryConflict=[&]{++conflicts;};
    juce::TextButton* button=nullptr;
    for(int i=0;i<editor.getNumChildComponents();++i)
        if(auto* found=dynamic_cast<juce::TextButton*>(editor.getChildComponent(i)))
            if(found->getButtonText()=="DUPLICATE NOTE")button=found;
    require(button && button->getButtonText()=="DUPLICATE NOTE" && !button->isEnabled(),"duplicate affordance");
    require(!editor.duplicateSelection(),"unselected duplication");select(editor);
    require(button->isEnabled(),"selected duplicate disabled");button->onClick();
    require(s.tracks[0].notes.size()==3,"first repeat not created");
    auto repeated=s.tracks[0].notes.back();
    require(repeated.pitch==64 && repeated.startBeat==1.25 && repeated.lengthBeats==0.75 && repeated.velocity==73,"repeat metadata");
    button->onClick();require(s.tracks[0].notes.size()==4 && s.tracks[0].notes.back().startBeat==2,"new repeat not selected");
    require(sameMidiNotes({s.tracks[0].notes[0],s.tracks[0].notes[1]},original),"original notes changed");
    // Preserve unrelated work added after duplication.
    s.bpm=120;s.tracks[0].gain=0.6f;require(s.addClip(1,{"keep.wav",0,1,0}),"new audio work");
    require(editor.restoreHistory(true) && s.tracks[0].notes.size()==3,"repeat undo");
    require(editor.restoreHistory(true) && sameMidiNotes(s.tracks[0].notes,original),"second repeat undo");
    require(s.bpm==120 && s.masterMix.gain==0.71 && s.tracks[0].gain==0.6f && s.tracks[1].clips.size()==1,"undo changed unrelated work");
    require(editor.restoreHistory(false) && editor.restoreHistory(false) && s.tracks[0].notes.size()==4,"repeat redo");
    const auto duplicated=s.tracks[0].notes;
    allowed=false;select(editor);require(!editor.duplicateSelection() && sameMidiNotes(duplicated,s.tracks[0].notes),"busy duplication changed notes");
    allowed=true;select(editor);editor.mouseDown(mouse(editor,63,260));
    require(!editor.duplicateSelection(),"duplicate during drag");editor.mouseUp(mouse(editor,63,260));
    select(editor);s.tracks[0].notes[0].velocity=1;const auto newer=s.tracks[0].notes;
    require(!editor.duplicateSelection() && sameMidiNotes(newer,s.tracks[0].notes) && conflicts==1 && !button->isEnabled(),"stale selection duplication");
    s.tracks[0].notes=duplicated;editor.resetEditor();
    require(!editor.duplicateSelection() && !button->isEnabled(),"project reset selection");
    editor.setSize(400,480);require(!button->isVisible(),"narrow footer duplicate overlaps velocity");
    editor.setSize(1000,480);require(button->isVisible() && button->getX()>110,"wide footer duplicate missing");
    select(editor);
    if(argc>1){
        juce::Image image(juce::Image::ARGB,1000,480,true,juce::SoftwareImageType{});
        {juce::Graphics g(image);editor.paintEntireComponent(g,true);}
        auto output=juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(argv[1])).createOutputStream();
        require(output!=nullptr,"preview stream");juce::PNGImageFormat png;require(png.writeImageToStream(image,*output),"preview render");
    }
    // Edge-of-view duplication follows the new selection without modifying timing.
    Session edge;edge.addTrack(TrackType::midi);edge.addMidiNote(0,{64,58,1,90});
    editor.session=&edge;editor.resetEditor();editor.setSize(1000,480);select(editor,983);
    require(editor.duplicateSelection() && edge.tracks[0].notes.back().startBeat==59,"edge duplicate");
    juce::Image image(juce::Image::ARGB,1000,480,true,juce::SoftwareImageType{});
    {juce::Graphics g(image);editor.paint(g);}
    require(image.getPixelAt(990,PianoRollViewport::headerHeight+20*13+4)==juce::Colours::gold,"new repeat not visible");
    // Invalid timing/capacity must leave the model untouched.
    Session invalid;invalid.addTrack(TrackType::midi);invalid.addTrack(TrackType::audio);
    require(!invalid.duplicateMidiNote(0,0) && !invalid.duplicateMidiNote(1,0) && !invalid.duplicateMidiNote(9,0),"invalid target accepted");
    for(const auto& n:std::vector<MidiNote>{{64,0,0,73},{64,1e300,1e-300,73},{64,1e308,1e308,73},
            {128,0,1,73},{64,0,1,0},{64,std::numeric_limits<double>::infinity(),1,73}}){
        invalid.tracks[0].notes={n};require(!invalid.duplicateMidiNote(0,0) && invalid.tracks[0].notes.size()==1,"invalid duplicate changed model");
    }
    invalid.tracks[0].notes.assign(100000,{64,0,1,73});require(!invalid.duplicateMidiNote(0,0) && invalid.tracks[0].notes.size()==100000,"capacity overflow");
    // Save/reopen, export MIDI events, and compare each repeated note's actual WAV.
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-midi-duplicate","",false);
    require(root.createDirectory().wasOk(),"fixture directory");const auto project=root.getChildFile("Session.idwproj");
    require(saveSession(s,project),"save repeated arrangement");Session reopened;
    require(loadSession(project,reopened) && sameMidiNotes(duplicated,reopened.tracks[0].notes),"reopen repeat data");
    juce::String error;const auto midi=root.getChildFile("repeats.mid");require(exportSessionMidi(reopened,midi,error),"export repeats");
    juce::MidiFile file;{juce::FileInputStream input(midi);require(file.readFrom(input,false),"MIDI read");}
    std::vector<int> onsets;
    for(int i=0;i<file.getTrack(1)->getNumEvents();++i){const auto& m=file.getTrack(1)->getEventPointer(i)->message;
        if(m.isNoteOn() && m.getNoteNumber()==64){require(m.getVelocity()==73,"repeat MIDI velocity");onsets.push_back((int)m.getTimeStamp());}}
    require(onsets==std::vector<int>{480,1200,1920},"repeat MIDI timing");
    const auto wav=root.getChildFile("repeats.wav");require(renderProjectWav(reopened,wav,error,48000),"repeat bounce");
    juce::WavAudioFormat format;auto reader=std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(wav.createInputStream().release(),true));
    require(reader && reader->lengthInSamples==96000,"repeat bounce duration");juce::AudioBuffer<float> audio(2,96000);
    require(reader->read(&audio,0,96000,0,true,true),"repeat bounce read");reader.reset();
    for(int offset:{0,18000,36000})for(int i=0;i<18000;++i)
        require(std::abs(audio.getSample(0,12000+i)-audio.getSample(0,12000+offset+i))<0.000001f,"repeat WAV samples differ");
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS note duplication button/selection, undo/redo, busy/stale/drag guards, overflow/capacity, visibility, save/reopen, MIDI and WAV parity\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
