#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& editor,int x,int y){
    const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{(float)x,(float)y},
            juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&editor,&editor,
            now,{(float)x,(float)y},now,1,false};
}
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    Session s;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);
    IDWPianoRoll editor;editor.session=&s;editor.setSize(1000,480);
    bool allowed=true;int changes=0,conflicts=0;
    editor.canEdit=[&]{return allowed;};editor.onChanged=[&]{++changes;};
    editor.onHistoryConflict=[&]{++conflicts;};
    // Use the production component's event handlers without opening a window.
    editor.mouseDown(mouse(editor,70,312));editor.mouseUp(mouse(editor,70,312));
    require(s.tracks[0].notes.size()==1 && s.tracks[0].notes[0].startBeat==1 &&
            s.tracks[0].notes[0].pitch==60,"draw note");
    const auto drawn=s.tracks[0].notes;
    editor.mouseDown(mouse(editor,71,312));
    editor.mouseDrag(mouse(editor,75,299));editor.mouseDrag(mouse(editor,79,286));
    require(!editor.restoreHistory(true),"undo allowed during drag");
    editor.mouseUp(mouse(editor,79,286));
    const auto moved=s.tracks[0].notes;
    require(moved[0].startBeat==1.5 && moved[0].pitch==62,"drag pitch/time");
    // Preserve unrelated work made after the gesture.
    s.bpm=93;s.tracks[0].gain=0.4f;s.tracks[0].synthPreset=4;
    require(s.addClip(1,{"untouched.wav",0,1,0}),"new audio take");
    require(editor.restoreHistory(true) && sameMidiNotes(s.tracks[0].notes,drawn),"one undo per drag");
    require(s.bpm==93 && s.tracks[0].gain==0.4f && s.tracks[0].synthPreset==4 &&
            s.tracks[1].clips.size()==1,"undo changed unrelated work");
    // Selecting without moving must retain redo.
    editor.mouseDown(mouse(editor,71,312));editor.mouseUp(mouse(editor,71,312));
    require(editor.restoreHistory(false) && sameMidiNotes(s.tracks[0].notes,moved),"selection cleared redo");
    // Resize uses the right edge and coalesces multiple updates.
    editor.mouseDown(mouse(editor,92,286));editor.mouseDrag(mouse(editor,96,286));
    editor.mouseDrag(mouse(editor,100,286));editor.mouseUp(mouse(editor,100,286));
    require(s.tracks[0].notes[0].lengthBeats==1.5,"resize length");
    require(editor.restoreHistory(true) && sameMidiNotes(s.tracks[0].notes,moved),"resize undo");
    require(editor.restoreHistory(false) && s.tracks[0].notes[0].lengthBeats==1.5,"resize redo");
    // Select and delete through the same operation used by Delete/Backspace.
    editor.mouseDown(mouse(editor,79,286));editor.mouseUp(mouse(editor,79,286));
    allowed=false;require(!editor.deleteSelection() && !editor.restoreHistory(true),"busy editor mutated");
    const auto beforeBlocked=s.tracks[0].notes;
    editor.mouseDown(mouse(editor,200,100));editor.mouseDrag(mouse(editor,240,120));
    editor.mouseUp(mouse(editor,240,120));
    require(sameMidiNotes(beforeBlocked,s.tracks[0].notes),"busy draw/drag mutated");
    allowed=true;
    editor.mouseDown(mouse(editor,79,286));editor.mouseUp(mouse(editor,79,286));
    const auto resized=s.tracks[0].notes;
    require(editor.deleteSelection() && s.tracks[0].notes.empty(),"delete note");
    require(editor.restoreHistory(true) && sameMidiNotes(s.tracks[0].notes,resized),"delete undo");
    // A new edit invalidates redo; an external change produces a safe conflict.
    editor.mouseDown(mouse(editor,200,100));editor.mouseUp(mouse(editor,200,100));
    require(!editor.restoreHistory(false),"new edit retained redo");
    s.tracks[0].notes[0].velocity=1;const auto external=s.tracks[0].notes;
    require(!editor.restoreHistory(true) && conflicts==1 && sameMidiNotes(external,s.tracks[0].notes),
            "conflicting notes overwritten");
    // Loading another project clears stale selection, pending gestures and both stacks.
    editor.resetEditor();require(!editor.restoreHistory(true) && !editor.deleteSelection(),"project reset retained history");
    editor.mouseDown(mouse(editor,79,286));
    require(s.addMidiNote(0,{40,10,1,50}),"external edit during gesture");
    const auto duringDrag=s.tracks[0].notes;
    editor.mouseDrag(mouse(editor,100,286));editor.mouseUp(mouse(editor,100,286));
    require(sameMidiNotes(duringDrag,s.tracks[0].notes) && !editor.restoreHistory(true) && conflicts==2,
            "gesture overwrote or captured external note edit");
    editor.setTrackIndex(1);editor.mouseDown(mouse(editor,100,100));
    require(s.tracks[1].notes.empty() && !editor.restoreHistory(true),"audio track edited as MIDI");
    require(!editor.keyPressed(juce::KeyPress('Z',juce::ModifierKeys::ctrlModifier,0)),"unfocused key consumed");
    // Bound history to 64 edits, and reject renamed/replaced/removed targets.
    MidiEditHistory history;Session bounded;bounded.addTrack(TrackType::midi);
    for(int i=0;i<70;++i){const auto before=bounded.tracks[0].notes;
        require(bounded.addMidiNote(0,{60,(double)i,1,100}) && history.record(bounded,0,before),"bounded record");}
    for(int i=0;i<64;++i)require(history.undo(bounded)==MidiEditHistory::Result::applied,"bounded undo");
    require(history.undo(bounded)==MidiEditHistory::Result::empty && bounded.tracks[0].notes.size()==6,"history cap");
    bounded.tracks[0].name="Replacement";
    require(history.redo(bounded)==MidiEditHistory::Result::conflict,"renamed target accepted");
    bounded.tracks.clear();require(history.redo(bounded)==MidiEditHistory::Result::conflict,"removed target accepted");
    // The edited state remains a normal save/reopen project.
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("idw-midi-edit-history",".idwproj",false);
    require(saveSession(s,file),"save edited notes");Session reopened;
    require(loadSession(file,reopened) && sameMidiNotes(reopened.tracks[0].notes,s.tracks[0].notes),"reopen edited notes");
    require(file.deleteFile(),"fixture cleanup");require(changes>=8,"change notification missing");
    std::cout<<"PASS piano-roll draw/move/resize/delete undo-redo, gesture grouping, busy/focus guards, conflicts, preservation, reset, capacity, save/reopen\n";
}
