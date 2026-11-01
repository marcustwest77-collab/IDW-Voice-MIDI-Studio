#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "SessionModel.h"
#include "MidiEditHistory.h"
#include <functional>
#include <optional>

// Graphical editor for notes played by the built-in instruments.
class IDWPianoRoll final : public juce::Component {
public:
    idw::daw::Session* session=nullptr;
    size_t trackIndex=0;
    std::function<void()> onChanged;
    std::function<bool()> canEdit;
    std::function<void()> onHistoryConflict;
    IDWPianoRoll(){setWantsKeyboardFocus(true);}
    void resetEditor(){history.clear();gestureBefore.reset();gestureExpected.clear();dragging=false;selectedValid=false;repaint();}
    void setTrackIndex(size_t index){finishGesture();trackIndex=index;selectedValid=false;repaint();}
    void visibilityChanged() override {if(!isVisible())finishGesture();}
    bool restoreHistory(bool undo){
        if(dragging || !editingAllowed())return false;
        const auto result=undo?history.undo(*session):history.redo(*session);
        if(result==idw::daw::MidiEditHistory::Result::conflict){if(onHistoryConflict)onHistoryConflict();}
        if(result!=idw::daw::MidiEditHistory::Result::applied)return false;
        selectedValid=false;if(onChanged)onChanged();repaint();return true;
    }
    bool deleteSelection(){
        if(dragging || !editingAllowed() || !selectedValid)return false;
        const auto before=session->tracks[trackIndex].notes;
        const bool removed=session->removeMidiNote(trackIndex,selectedIndex);
        selectedValid=false;
        if(removed){history.record(*session,trackIndex,before);if(onChanged)onChanged();repaint();}
        return removed;
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff141b24));
        const auto bounds=getLocalBounds();
        constexpr int keyWidth=54,rowHeight=13;
        for(int pitch=84;pitch>=48;--pitch){
            const int y=(84-pitch)*rowHeight;
            g.setColour((pitch%12==1||pitch%12==3||pitch%12==6||
                         pitch%12==8||pitch%12==10)?juce::Colour(0xff1e2631):
                                                        juce::Colour(0xff29323e));
            g.fillRect(0,y,bounds.getWidth(),rowHeight-1);
            g.setColour(juce::Colours::lightgrey);
            if(pitch%12==0)g.drawText("C"+juce::String(pitch/12-1),2,y,48,rowHeight,juce::Justification::centredLeft);
        }
        for(int beat=0;beat<64;++beat){
            const int x=keyWidth+beat*16;
            if(x>getWidth())break;
            g.setColour(beat%4==0?juce::Colours::grey:juce::Colour(0xff435064));
            g.drawVerticalLine(x,0,(float)getHeight());
        }
        if(!session || trackIndex>=session->tracks.size())return;
        size_t noteIndex=0;
        for(const auto& note:session->tracks[trackIndex].notes){
            ++noteIndex;
            if(note.pitch<48 || note.pitch>84)continue;
            const int y=(84-note.pitch)*rowHeight;
            const int x=keyWidth+juce::roundToInt(note.startBeat*16.0);
            const int w=juce::jmax(3,juce::roundToInt(note.lengthBeats*16.0));
            g.setColour(selectedValid && noteIndex-1==selectedIndex ? juce::Colours::gold : juce::Colours::orange);
            g.fillRoundedRectangle((float)x,(float)y+1,(float)w,rowHeight-3,2.0f);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        finishGesture();
        selectedValid=false;
        if(!editingAllowed() ||
           session->tracks[trackIndex].type!=idw::daw::TrackType::midi ||
           e.x<54 || e.y<0 || e.y>=37*13)return;
        auto& notes=session->tracks[trackIndex].notes;
        for(size_t i=notes.size();i>0;--i){
            const auto& n=notes[i-1];
            const int x=54+juce::roundToInt(n.startBeat*16.0);
            const int y=(84-n.pitch)*13;
            const int w=juce::jmax(3,juce::roundToInt(n.lengthBeats*16.0));
            if(e.x>=x && e.x<x+w && e.y>=y && e.y<y+13){
                activeNote=i-1;selectedIndex=activeNote;selectedValid=true;
                grabKeyboardFocus();dragging=true;resizing=e.x>=x+w-6;
                originX=e.x;originY=e.y;original=notes[activeNote];
                gestureTrack=trackIndex;gestureBefore=notes;gestureExpected=notes;
                gestureTrackName=session->tracks[trackIndex].name;
                repaint();
                return;
            }
        }
        const int pitch=84-e.y/13;
        const double beat=std::max(0.0,std::round(((double)e.x-54)/16.0*4.0)/4.0);
        const auto before=notes;
        if(session->addMidiNote(trackIndex,{pitch,beat,1.0,100})){
            history.record(*session,trackIndex,before);
            selectedIndex=notes.size()-1;selectedValid=true;grabKeyboardFocus();
            if(onChanged)onChanged();
            repaint();
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(!dragging || !editingAllowed() || trackIndex!=gestureTrack)return;
        if(!gestureMatches()){
            gestureBefore.reset();dragging=false;selectedValid=false;
            if(onHistoryConflict)onHistoryConflict();return;
        }
        const double delta=std::round((e.x-originX)/4.0)/4.0;
        bool changed=false;
        if(resizing)
            changed=session->resizeMidiNote(trackIndex,activeNote,std::max(0.25,original.lengthBeats+delta));
        else
            changed=session->moveMidiNote(trackIndex,activeNote,
                std::max(0.0,original.startBeat+delta),
                juce::jlimit(0,127,original.pitch-(e.y-originY)/13));
        if(changed){gestureExpected=session->tracks[trackIndex].notes;if(onChanged)onChanged();repaint();}
    }
    void mouseUp(const juce::MouseEvent&) override {finishGesture();}
    bool keyPressed(const juce::KeyPress& key) override {
        if(!hasKeyboardFocus(false) || dragging || !editingAllowed())return false;
        const auto mods=key.getModifiers();
        if(mods.isCommandDown() && !mods.isAltDown() &&
           (key.getKeyCode()=='Z' || (key.getKeyCode()=='Y' && !mods.isShiftDown()))){
            const bool undo=key.getKeyCode()=='Z' && !mods.isShiftDown();
            restoreHistory(undo);
            return true;
        }
        if(!selectedValid || mods.isAnyModifierKeyDown())return false;
        if(key.getKeyCode()!=juce::KeyPress::deleteKey && key.getKeyCode()!=juce::KeyPress::backspaceKey)return false;
        return deleteSelection();
    }
private:
    idw::daw::MidiEditHistory history;
    std::optional<std::vector<idw::daw::MidiNote>> gestureBefore;
    std::vector<idw::daw::MidiNote> gestureExpected;
    std::string gestureTrackName;
    size_t gestureTrack=0;
    bool editingAllowed() const {
        return session && trackIndex<session->tracks.size() &&
            session->tracks[trackIndex].type==idw::daw::TrackType::midi && (!canEdit || canEdit());
    }
    void finishGesture(){
        if(gestureBefore){
            if(gestureMatches())history.record(*session,gestureTrack,*gestureBefore);
            else if(onHistoryConflict)onHistoryConflict();
        }
        gestureBefore.reset();dragging=false;
    }
    bool gestureMatches() const {
        return session && gestureTrack<session->tracks.size() &&
            session->tracks[gestureTrack].type==idw::daw::TrackType::midi &&
            session->tracks[gestureTrack].name==gestureTrackName &&
            idw::daw::sameMidiNotes(session->tracks[gestureTrack].notes,gestureExpected);
    }
    bool selectedValid=false;
    size_t selectedIndex=0;
    bool dragging=false,resizing=false;
    size_t activeNote=0;
    int originX=0,originY=0;
    idw::daw::MidiNote original;
};
