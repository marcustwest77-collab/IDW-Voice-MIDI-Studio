#include "PianoRoll.h"
#include "SessionPersistence.h"
#include "OfflineBounce.h"
#include "StandardMidiExport.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& e,int x,int y){
    y+=PianoRollViewport::headerHeight;const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{(float)x,(float)y},
        juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&e,&e,now,{(float)x,(float)y},now,1,false};
}
static void select(IDWPianoRoll& e,const MidiNote& n){
    const int x=55+juce::roundToInt(n.startBeat*16),y=(84-n.pitch)*13;
    e.mouseDown(mouse(e,x,y));e.mouseUp(mouse(e,x,y));
}
static bool arrow(IDWPianoRoll& e,int code,bool shift=false){
    const auto command=pianoRollNudge(juce::KeyPress(code,shift?juce::ModifierKeys::shiftModifier:0,0));
    require(command.has_value(),"arrow not decoded");return e.nudgeSelection(command->beats,command->semitones);
}
int main(){try{
    juce::ScopedJuceInitialiser_GUI init;
    for(bool coarse:{false,true})for(int code:{juce::KeyPress::leftKey,juce::KeyPress::rightKey,juce::KeyPress::upKey,juce::KeyPress::downKey}){
        const auto command=pianoRollNudge(juce::KeyPress(code,coarse?juce::ModifierKeys::shiftModifier:0,0));
        require(command.has_value(),"editing arrow ignored");
        const double beats=(code==juce::KeyPress::leftKey?-1:code==juce::KeyPress::rightKey?1:0)*(coarse?1.0:0.25);
        const int pitch=(code==juce::KeyPress::downKey?-1:code==juce::KeyPress::upKey?1:0)*(coarse?12:1);
        require(command->beats==beats && command->semitones==pitch,"arrow mapping");
        for(int mods:{juce::ModifierKeys::ctrlModifier,juce::ModifierKeys::commandModifier,juce::ModifierKeys::altModifier})
            require(!pianoRollNudge(juce::KeyPress(code,mods,0)),"modifier arrow stolen");
    }
    require(!pianoRollNudge(juce::KeyPress('A',0,0)),"non-arrow decoded");
    Session s;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);s.tracks[1].mute=true;
    require(s.addMidiNote(0,{60,0.5,1,87}),"note fixture");
    IDWPianoRoll editor;editor.session=&s;editor.setSize(1000,480);
    bool allowed=true;int conflicts=0;editor.canEdit=[&]{return allowed;};editor.onHistoryConflict=[&]{++conflicts;};
    require(!editor.nudgeSelection(0.25,0),"unselected nudge");select(editor,s.tracks[0].notes[0]);
    require(editor.nudgeSelection(0.25,1) && s.tracks[0].notes[0].startBeat==0.75 && s.tracks[0].notes[0].pitch==61,"combined nudge");
    require(s.tracks[0].notes[0].lengthBeats==1 && s.tracks[0].notes[0].velocity==87,"nudge changed length/velocity");
    require(editor.nudgeSelection(-0.25,-1),"reverse nudge");
    s.bpm=93;s.masterMix.gain=0.8;s.tracks[0].gain=0.6f;s.addClip(1,{"preserved.wav",0,1,0});
    require(editor.restoreHistory(true) && s.tracks[0].notes[0].pitch==61,"nudge undo");
    select(editor,s.tracks[0].notes[0]);require(!editor.nudgeSelection(0,0),"no-op nudge accepted");
    require(editor.restoreHistory(false) && s.tracks[0].notes[0].pitch==60,"no-op cleared redo");
    require(s.bpm==93 && s.masterMix.gain==0.8 && s.tracks[0].gain==0.6f && s.tracks[1].clips.size()==1,"undo changed unrelated work");
    select(editor,s.tracks[0].notes[0]);const auto current=s.tracks[0].notes;
    require(!editor.keyPressed(juce::KeyPress(juce::KeyPress::rightKey,0,0)) && sameMidiNotes(current,s.tracks[0].notes),"unfocused arrow edited notes");
    allowed=false;require(!editor.nudgeSelection(0.25,0) && sameMidiNotes(current,s.tracks[0].notes),"busy nudge");allowed=true;
    editor.mouseDown(mouse(editor,63,312));require(!editor.nudgeSelection(0.25,0),"nudge during note drag");editor.mouseUp(mouse(editor,63,312));
    juce::Slider* velocity=nullptr;for(int i=0;i<editor.getNumChildComponents();++i)
        if(auto* slider=dynamic_cast<juce::Slider*>(editor.getChildComponent(i)))velocity=slider;
    require(velocity!=nullptr,"velocity control");velocity->onDragStart();
    require(!editor.nudgeSelection(0,1),"nudge during velocity drag");velocity->onDragEnd();
    s.tracks[0].notes[0].velocity=1;const auto newer=s.tracks[0].notes;
    require(!editor.nudgeSelection(0,1) && sameMidiNotes(newer,s.tracks[0].notes) && conflicts==1,"stale nudge overwrote notes");
    // Checked model behavior at the timeline/pitch limits and invalid inputs.
    Session bounds;bounds.addTrack(TrackType::midi);bounds.addTrack(TrackType::audio);
    bounds.tracks[0].notes={{0,0.1,1,100}};
    require(bounds.nudgeMidiNote(0,0,-0.25,0) && bounds.tracks[0].notes[0].startBeat==0,"negative timeline not clamped");
    require(!bounds.nudgeMidiNote(0,0,-0.25,0) && !bounds.nudgeMidiNote(0,0,0,-1),"boundary no-op or negative pitch accepted");
    bounds.tracks[0].notes[0].pitch=120;
    require(!bounds.nudgeMidiNote(0,0,0,12) && bounds.tracks[0].notes[0].pitch==120,"partial octave applied");
    require(!bounds.nudgeMidiNote(0,0,0,std::numeric_limits<int>::max()),"pitch integer overflow");
    require(!bounds.nudgeMidiNote(99,0,1,0) && !bounds.nudgeMidiNote(1,0,1,0) && !bounds.nudgeMidiNote(0,99,1,0),"invalid target");
    for(double delta:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        require(!bounds.nudgeMidiNote(0,0,delta,0),"nonfinite nudge");
    for(const auto& n:std::vector<MidiNote>{{60,1e308,1e308,100},{60,1e300,1e-300,100},{60,0,0,100},{128,0,1,100},{60,0,1,0}}){
        bounds.tracks[0].notes={n};require(!bounds.nudgeMidiNote(0,0,1e308,1) && bounds.tracks[0].notes.size()==1,"invalid/overflowing note moved");}
    // Move beyond the initial pitch view and verify the selected note is painted.
    Session edge;edge.addTrack(TrackType::midi);edge.addMidiNote(0,{84,58,1,100});
    editor.session=&edge;editor.resetEditor();select(editor,edge.tracks[0].notes[0]);
    require(arrow(editor,juce::KeyPress::upKey) && arrow(editor,juce::KeyPress::rightKey,true),"edge nudge");
    juce::Image image(juce::Image::ARGB,1000,480,true,juce::SoftwareImageType{});
    {juce::Graphics g(image);editor.paint(g);}
    require(image.getPixelAt(990,PianoRollViewport::headerHeight+4)==juce::Colours::gold,"nudge did not reveal selected pitch/time");
    // Production command decoding -> note edit -> persistence -> MIDI/WAV output.
    Session rendered;rendered.addTrack(TrackType::midi);rendered.addMidiNote(0,{60,1,1,87});
    editor.session=&rendered;editor.resetEditor();select(editor,rendered.tracks[0].notes[0]);
    require(arrow(editor,juce::KeyPress::rightKey,true) && arrow(editor,juce::KeyPress::upKey) && arrow(editor,juce::KeyPress::upKey,true),"arrow operations");
    const auto note=rendered.tracks[0].notes[0];require(note.startBeat==2 && note.pitch==73 && note.lengthBeats==1 && note.velocity==87,"edited note contents");
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-midi-nudge","",false);
    require(root.createDirectory().wasOk(),"fixture directory");const auto project=root.getChildFile("Session.idwproj");
    require(saveSession(rendered,project),"save nudged project");Session reopened;
    require(loadSession(project,reopened) && sameMidiNotes(reopened.tracks[0].notes,rendered.tracks[0].notes),"reopen nudged project");
    juce::String error;const auto midi=root.getChildFile("nudged.mid"),wav=root.getChildFile("nudged.wav");
    require(exportSessionMidi(reopened,midi,error) && renderProjectWav(reopened,wav,error,48000),"nudged export");
    juce::MidiFile file;{juce::FileInputStream input(midi);require(file.readFrom(input,false),"MIDI read");}
    bool on=false,off=false;for(int i=0;i<file.getTrack(1)->getNumEvents();++i){const auto& m=file.getTrack(1)->getEventPointer(i)->message;
        if(m.isNoteOn()){require(m.getTimeStamp()==1920 && m.getNoteNumber()==73 && m.getVelocity()==87,"nudged MIDI note-on");on=true;}
        if(m.isNoteOff()){require(m.getTimeStamp()==2880 && m.getNoteNumber()==73,"nudged MIDI note-off");off=true;}}
    require(on && off,"missing MIDI note pair");
    juce::WavAudioFormat format;auto reader=std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(wav.createInputStream().release(),true));
    require(reader && reader->lengthInSamples==72000,"nudged WAV duration");juce::AudioBuffer<float> audio(2,72000);
    require(reader->read(&audio,0,72000,0,true,true),"WAV read");reader.reset();
    require(audio.getMagnitude(0,48000)==0,"nudged onset too early");
    for(int i=1000;i<2000;++i){const float expected=0.12f*(87/127.0f)*(float)std::sin(i*6.283185307179586*554.3652619537442/48000);
        require(std::abs(audio.getSample(0,48000+i)-expected)<0.000001f,"nudged pitch/time WAV oracle");}
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS MIDI arrow decoding, selected-note nudge/undo/redo, bounds, focus/busy/drag/conflict guards, visibility, persistence, MIDI and WAV oracle\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
