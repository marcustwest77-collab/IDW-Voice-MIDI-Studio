#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "SessionModel.h"
#include "MidiEditHistory.h"
#include "MidiQuantize.h"
#include "MidiTranspose.h"
#include "PianoRollViewport.h"
#include "PianoRollKeyCommands.h"
#include <functional>
#include <optional>

// Graphical editor for notes played by the built-in instruments.
class IDWPianoRoll final : public juce::Component, private juce::Timer {
public:
    idw::daw::Session* session=nullptr;
    size_t trackIndex=0;
    std::function<void()> onChanged;
    std::function<bool()> canEdit;
    std::function<void()> onHistoryConflict;
    static constexpr int velocityFooterHeight=30;
    IDWPianoRoll(){
        setWantsKeyboardFocus(true);
        closeEditor.setButtonText("BACK TO TRACKS");
        closeEditor.setTooltip("Close the piano roll without changing notes.");
        closeEditor.onClick=[this]{setVisible(false);};
        addAndMakeVisible(closeEditor);
        undoEdit.setButtonText("UNDO MIDI");redoEdit.setButtonText("REDO MIDI");
        for(auto* button:{&undoEdit,&redoEdit}){button->setWantsKeyboardFocus(false);addAndMakeVisible(*button);button->setEnabled(false);}
        undoEdit.setTooltip("Undo a MIDI note edit. Reselect a note to inspect its restored value.");
        redoEdit.setTooltip("Redo a MIDI note edit. Reselect a note to inspect its restored value.");
        undoEdit.onClick=[this]{restoreFromButton(true);};
        redoEdit.onClick=[this]{restoreFromButton(false);};
        quantize.setButtonText("QUANTIZE TRACK");quantize.setWantsKeyboardFocus(false);
        quantize.setTooltip("Snap every note start on this track to a chosen grid. Length, pitch and velocity stay unchanged; one MIDI undo step.");
        addAndMakeVisible(quantize);quantize.setEnabled(false);
        quantize.onClick=[this]{showQuantizeMenu();};
        transpose.setButtonText("TRANSPOSE TRACK");transpose.setWantsKeyboardFocus(false);
        transpose.setTooltip("Move every note on this track by semitones or an octave. Timing and velocity stay unchanged; one MIDI undo step. Out-of-range requests leave all notes unchanged.");
        addAndMakeVisible(transpose);transpose.setEnabled(false);
        transpose.onClick=[this]{showTransposeMenu();};
        editFeedback.setFont(juce::Font(juce::FontOptions(12.0f)));
        editFeedback.setComponentID("midi-edit-feedback");
        editFeedback.setColour(juce::Label::textColourId,juce::Colours::white);
        editFeedback.setTooltip("Result of the last MIDI action, including the first two note starts in zero-based beats (rounded to three decimals).");
        addAndMakeVisible(editFeedback);
        velocityLabel.setText("NOTE VELOCITY",juce::dontSendNotification);addAndMakeVisible(velocityLabel);
        velocity.setRange(1,127,1);velocity.setValue(100,juce::dontSendNotification);
        velocity.setTextBoxStyle(juce::Slider::TextBoxRight,false,50,24);
        velocity.valueFromTextFunction=[this](const juce::String& text){
            const auto number=text.trim();
            if(number.isEmpty() || number.length()>3 || !number.containsOnly("0123456789"))return velocity.getValue();
            const int value=number.getIntValue();
            return value>=1 && value<=127 ? (double)value : velocity.getValue();
        };
        velocity.setWantsKeyboardFocus(false); // Keep piano-roll undo after a thumb drag; text entry owns its own focus.
        velocity.setScrollWheelEnabled(false);velocity.setTooltip("Select a note, then set its velocity (1–127). Dragging is one undo step.");
        addAndMakeVisible(velocity);velocity.setEnabled(false);
        duplicate.setButtonText("DUPLICATE NOTE");duplicate.setWantsKeyboardFocus(false);
        duplicate.setTooltip("Repeat the selected note immediately after itself (Ctrl/Cmd+D).");
        addAndMakeVisible(duplicate);duplicate.setEnabled(false);
        duplicate.onClick=[this]{duplicateSelection();};
        halfLength.setButtonText("HALF LENGTH");doubleLength.setButtonText("2x LENGTH");
        for(auto* button:{&halfLength,&doubleLength}){
            button->setWantsKeyboardFocus(false);addAndMakeVisible(*button);button->setEnabled(false);
        }
        halfLength.setTooltip("Halve selected note duration ([). Keeps start, pitch and velocity.");
        doubleLength.setTooltip("Double selected note duration (]). Keeps start, pitch and velocity.");
        halfLength.onClick=[this]{scaleSelectionLength(0.5);};
        doubleLength.onClick=[this]{scaleSelectionLength(2.0);};
        velocity.onDragStart=[this]{
            finishGesture();
            if(!editingAllowed() || !selectionMatches())return;
            gestureTrack=trackIndex;gestureBefore=selectedSnapshot;gestureExpected=selectedSnapshot;
            gestureTrackName=selectedTrackName;dragging=true;velocityDragging=true;
            refreshVelocity();
        };
        velocity.onValueChange=[this]{setSelectionVelocity((int)velocity.getValue());};
        velocity.onDragEnd=[this]{finishGesture();};
    }
    void resetEditor(){++editGeneration;history.clear();gestureBefore.reset();gestureExpected.clear();selectedSnapshot.clear();dragging=false;velocityDragging=false;selectedValid=false;view={};setVisible(false);constrainView();refreshVelocity();showEditFeedback("No MIDI edits yet");repaint();}
    void trackRenamed(size_t index,const std::string& previous){
        if(!session || index>=session->tracks.size())return;
        history.trackRenamed(index,previous,session->tracks[index].name);
        if(index==trackIndex){selectedValid=false;selectedSnapshot.clear();refreshVelocity();repaint();}
    }
    // Closing must work after track selection changes, including an audio track
    // or an unavailable index. Only opening requires a valid MIDI destination.
    bool toggleForTrack(size_t index){
        if(isVisible()){setVisible(false);return true;}
        if(!session || index>=session->tracks.size() || session->tracks[index].type!=idw::daw::TrackType::midi)return false;
        setTrackIndex(index);setVisible(true);return true;
    }
    void setTrackIndex(size_t index){const bool changed=index!=trackIndex;++editGeneration;finishGesture();trackIndex=index;selectedValid=false;constrainView();refreshVelocity();if(changed)showEditFeedback("Track selected");repaint();}
    void resized() override {
        finishGesture();constrainView();
        closeEditor.setBounds(juce::jmax(0,getWidth()-114),2,juce::jmin(110,getWidth()),20);
        const bool showHistory=getWidth()>=340;
        undoEdit.setVisible(showHistory);redoEdit.setVisible(showHistory);
        undoEdit.setBounds(4,2,90,20);redoEdit.setBounds(98,2,90,20);
        quantize.setVisible(getWidth()>=450);quantize.setBounds(192,2,132,20);
        transpose.setVisible(getWidth()>=600);transpose.setBounds(328,2,142,20);
        editFeedback.setVisible(getWidth()>=1000);editFeedback.setBounds(474,2,std::max(1,getWidth()-592),20);
        velocityLabel.setBounds(4,getHeight()-28,104,24);
        const bool showDuplicate=getWidth()>=480,showLength=getWidth()>=760;
        duplicate.setVisible(showDuplicate);halfLength.setVisible(showLength);doubleLength.setVisible(showLength);
        int right=getWidth()-4;
        duplicate.setBounds(right-130,getHeight()-28,130,24);if(showDuplicate)right-=136;
        doubleLength.setBounds(right-96,getHeight()-28,96,24);
        halfLength.setBounds(right-202,getHeight()-28,100,24);if(showLength)right-=208;
        velocity.setBounds(110,getHeight()-28,std::max(1,right-110),24);
    }
    void visibilityChanged() override {
        if(isVisible())startTimerHz(20);
        else {stopTimer();finishGesture();}
    }
    // Recover a gesture still marked active after its physical release.
    // Finalize its existing edit only when no mouse button is held.
    void finishReleasedGesture(bool mouseButtonDown){
        if(dragging && !mouseButtonDown)finishGesture();
    }
    bool restoreHistory(bool undo){
        if(dragging || !editingAllowed()){showEditFeedback("MIDI history unavailable while busy");return false;}
        const auto result=undo?history.undo(*session):history.redo(*session);
        if(result==idw::daw::MidiEditHistory::Result::conflict){if(onHistoryConflict)onHistoryConflict();}
        if(result!=idw::daw::MidiEditHistory::Result::applied){
            showEditFeedback(result==idw::daw::MidiEditHistory::Result::conflict?"MIDI history conflict":undo?"No MIDI Undo available":"No MIDI Redo available");return false;
        }
        selectedValid=false;refreshVelocity();if(onChanged)onChanged();showEditFeedback(undo?"Undo applied":"Redo applied");repaint();return true;
    }
    bool quantizeTrack(int divisions){
        if(dragging || !editingAllowed()){showEditFeedback("Quantize unavailable while busy");return false;}
        const auto before=session->tracks[trackIndex].notes;
        std::vector<idw::daw::MidiNote> next;
        const auto result=idw::daw::quantizeMidiNotes(before,divisions,next);
        if(result!=idw::daw::MidiQuantizeResult::changed){
            showEditFeedback(result==idw::daw::MidiQuantizeResult::unchanged?"Already on grid":"Quantize unavailable");return false;
        }
        session->tracks[trackIndex].notes.swap(next);history.record(*session,trackIndex,before);
        selectedValid=false;constrainView();refreshVelocity();if(onChanged)onChanged();showEditFeedback("Quantize applied");repaint();return true;
    }
    bool transposeTrack(int semitones){
        if(dragging || !editingAllowed()){showEditFeedback("Transpose unavailable while busy");return false;}
        const auto before=session->tracks[trackIndex].notes;
        std::vector<idw::daw::MidiNote> next;
        const auto result=idw::daw::transposeMidiNotes(before,semitones,next);
        if(result!=idw::daw::MidiTransposeResult::changed){
            showEditFeedback(result==idw::daw::MidiTransposeResult::unchanged?"Transpose unchanged":"Transpose outside valid note range");return false;
        }
        session->tracks[trackIndex].notes.swap(next);history.record(*session,trackIndex,before);
        selectedValid=false;constrainView();refreshVelocity();if(onChanged)onChanged();
        showEditFeedback("Transpose "+(semitones>0?juce::String("+"):juce::String{})+juce::String(semitones)+" applied");repaint();return true;
    }
    bool setSelectionVelocity(int value){
        if((dragging && !velocityDragging) || !editingAllowed() || value<1 || value>127){refreshVelocity();return false;}
        if(!selectionMatches()){
            if(selectedValid && onHistoryConflict)onHistoryConflict();
            if(velocityDragging){gestureBefore.reset();dragging=false;velocityDragging=false;}
            selectedValid=false;refreshVelocity();return false;
        }
        const auto before=session->tracks[trackIndex].notes;
        if(before[selectedIndex].velocity==value){refreshVelocity();return false;}
        if(!session->setMidiNoteVelocity(trackIndex,selectedIndex,value))return false;
        rememberSelection();
        if(velocityDragging)gestureExpected=selectedSnapshot;
        else history.record(*session,trackIndex,before);
        refreshVelocity();if(onChanged)onChanged();repaint();return true;
    }
    bool deleteSelection(){
        if(dragging || !editingAllowed() || !selectionMatches())return false;
        const auto before=session->tracks[trackIndex].notes;
        const bool removed=session->removeMidiNote(trackIndex,selectedIndex);
        selectedValid=false;refreshVelocity();
        if(removed){history.record(*session,trackIndex,before);if(onChanged)onChanged();repaint();}
        return removed;
    }
    bool duplicateSelection(){
        if(dragging || !editingAllowed()){refreshVelocity();return false;}
        if(!selectionMatches()){
            if(selectedValid && onHistoryConflict)onHistoryConflict();
            selectedValid=false;refreshVelocity();return false;
        }
        const auto before=session->tracks[trackIndex].notes;
        if(!session->duplicateMidiNote(trackIndex,selectedIndex))return false;
        history.record(*session,trackIndex,before);
        selectedIndex=session->tracks[trackIndex].notes.size()-1;selectedValid=true;
        rememberSelection();
        revealSelection();refreshVelocity();grabKeyboardFocus();
        if(onChanged)onChanged();repaint();return true;
    }
    bool nudgeSelection(double beats,int semitones){
        if(dragging || !editingAllowed())return false;
        if(!selectionMatches()){
            if(selectedValid && onHistoryConflict)onHistoryConflict();
            selectedValid=false;refreshVelocity();return false;
        }
        const auto before=session->tracks[trackIndex].notes;
        if(!session->nudgeMidiNote(trackIndex,selectedIndex,beats,semitones))return false;
        history.record(*session,trackIndex,before);rememberSelection();
        revealSelection();refreshVelocity();
        if(onChanged)onChanged();repaint();return true;
    }
    bool scaleSelectionLength(double factor){
        if(dragging || !editingAllowed())return false;
        if(!selectionMatches()){
            if(selectedValid && onHistoryConflict)onHistoryConflict();
            selectedValid=false;refreshVelocity();return false;
        }
        const auto before=session->tracks[trackIndex].notes;
        if(!session->scaleMidiNoteLength(trackIndex,selectedIndex,factor))return false;
        history.record(*session,trackIndex,before);rememberSelection();
        revealSelection();refreshVelocity();grabKeyboardFocus();
        if(onChanged)onChanged();repaint();return true;
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff141b24));
        const auto bounds=getLocalBounds();
        constrainView();
        constexpr int keyWidth=idw::daw::PianoRollViewport::keyWidth;
        constexpr int rowHeight=idw::daw::PianoRollViewport::rowHeight;
        constexpr int header=idw::daw::PianoRollViewport::headerHeight;
        for(int row=0;row<idw::daw::PianoRollViewport::rows(gridHeight());++row){
            const int pitch=view.topPitch-row,y=header+row*rowHeight;
            g.setColour((pitch%12==1||pitch%12==3||pitch%12==6||
                         pitch%12==8||pitch%12==10)?juce::Colour(0xff1e2631):
                                                        juce::Colour(0xff29323e));
            g.fillRect(0,y,bounds.getWidth(),rowHeight-1);
            g.setColour(juce::Colours::lightgrey);
            if(pitch%12==0)g.drawText("C"+juce::String(pitch/12-1),2,y,48,rowHeight,juce::Justification::centredLeft);
        }
        g.setColour(juce::Colours::lightgrey);g.setFont(juce::Font(juce::FontOptions(10.0f)));
        const int helpLeft=undoEdit.isVisible()?194:2;
        g.drawText("Arrows: move | Shift: beat/octave | [ ]: length | Wheel: pitch | Shift+wheel: time | Ctrl/Cmd+wheel: zoom",helpLeft,0,juce::jmax(0,closeEditor.getX()-helpLeft-6),12,juce::Justification::centredLeft);
        const double first=std::ceil(view.firstBeat);
        const int lines=(int)std::ceil(gridWidth()/view.pixelsPerBeat())+1;
        for(int i=0;i<lines;++i){
            const double beat=first+i;
            const int x=keyWidth+juce::roundToInt((beat-view.firstBeat)*view.pixelsPerBeat());
            if(x>getWidth())break;
            g.setColour(std::fmod(beat,4.0)==0?juce::Colours::grey:juce::Colour(0xff435064));
            g.drawVerticalLine(x,(float)header,(float)(header+gridHeight()));
            if(std::fmod(beat,4.0)==0)g.drawText(juce::String(beat,0),x+2,12,42,12,juce::Justification::centredLeft);
        }
        if(!session || trackIndex>=session->tracks.size())return;
        size_t noteIndex=0;
        for(const auto& note:session->tracks[trackIndex].notes){
            ++noteIndex;
            const auto rect=view.bounds(note,gridWidth(),gridHeight());
            if(!rect)continue;
            g.setColour(selectedValid && noteIndex-1==selectedIndex ? juce::Colours::gold : juce::Colours::orange);
            g.fillRoundedRectangle((float)(keyWidth+rect->x),(float)(header+rect->y)+1,
                                   (float)rect->width,rowHeight-3,2.0f);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        finishGesture();
        constrainView();
        selectedValid=false;refreshVelocity();
        if(!editingAllowed() ||
           e.x<54 || e.x>=getWidth())return;
        const auto pitch=view.pitchAt(e.y-idw::daw::PianoRollViewport::headerHeight,gridHeight());
        if(!pitch)return;
        auto& notes=session->tracks[trackIndex].notes;
        for(size_t i=notes.size();i>0;--i){
            const auto& n=notes[i-1];
            const auto rect=view.bounds(n,gridWidth(),gridHeight());
            if(!rect)continue;
            const double x=54+rect->x;const int y=idw::daw::PianoRollViewport::headerHeight+rect->y;
            if(e.x>=x && e.x<x+rect->width && e.y>=y && e.y<y+13){
                activeNote=i-1;selectedIndex=activeNote;selectedValid=true;
                grabKeyboardFocus();dragging=true;resizing=rect->rightEdgeVisible && e.x>=x+rect->width-6;
                originX=e.x;originY=e.y;original=notes[activeNote];
                gestureTrack=trackIndex;gestureBefore=notes;gestureExpected=notes;
                gestureTrackName=session->tracks[trackIndex].name;
                rememberSelection();refreshVelocity();
                repaint();
                return;
            }
        }
        const double beat=std::round(view.beatAt(e.x-54)*4.0)/4.0;
        const auto before=notes;
        if(session->addMidiNote(trackIndex,{*pitch,beat,1.0,100})){
            history.record(*session,trackIndex,before);
            selectedIndex=notes.size()-1;selectedValid=true;grabKeyboardFocus();
            rememberSelection();refreshVelocity();
            if(onChanged)onChanged();
            repaint();
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(!dragging || velocityDragging || !editingAllowed() || trackIndex!=gestureTrack)return;
        if(!gestureMatches()){
            gestureBefore.reset();dragging=false;selectedValid=false;
            refreshVelocity();
            if(onHistoryConflict)onHistoryConflict();return;
        }
        const double delta=std::round((e.x-originX)/view.pixelsPerBeat()*4.0)/4.0;
        bool changed=false;
        if(resizing)
            changed=session->resizeMidiNote(trackIndex,activeNote,std::max(0.25,original.lengthBeats+delta));
        else
            changed=session->moveMidiNote(trackIndex,activeNote,
                std::max(0.0,original.startBeat+delta),
                juce::jlimit(0,127,original.pitch-(e.y-originY)/13));
        if(changed){gestureExpected=session->tracks[trackIndex].notes;rememberSelection();refreshVelocity();if(onChanged)onChanged();repaint();}
    }
    void mouseUp(const juce::MouseEvent&) override {finishGesture();}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel) override {
        if(dragging)return;
        if(e.mods.isCommandDown())
            view.zoomAt(wheel.deltaY>0?1:wheel.deltaY<0?-1:0,e.x-54,gridWidth(),gridHeight(),lastBeat());
        else if(e.mods.isShiftDown() || std::abs(wheel.deltaX)>std::abs(wheel.deltaY))
            view.scrollTime(-16.0*(e.mods.isShiftDown() && wheel.deltaY!=0?wheel.deltaY:wheel.deltaX),
                            gridWidth(),gridHeight(),lastBeat());
        else view.scrollPitch(12.0*wheel.deltaY,gridHeight());
        repaint();
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(!hasKeyboardFocus(false) || dragging || !editingAllowed())return false;
        const auto mods=key.getModifiers();
        if(mods.isCommandDown() && !mods.isAltDown() &&
           (key.getKeyCode()=='Z' || (key.getKeyCode()=='Y' && !mods.isShiftDown()))){
            const bool undo=key.getKeyCode()=='Z' && !mods.isShiftDown();
            restoreHistory(undo);
            return true;
        }
        if(mods.isCommandDown() && !mods.isAltDown() && !mods.isShiftDown() && key.getKeyCode()=='D'){
            duplicateSelection();return true;
        }
        if(selectedValid)if(const auto nudge=idw::daw::pianoRollNudge(key)){
            nudgeSelection(nudge->beats,nudge->semitones);return true;
        }
        if(!selectedValid || mods.isAnyModifierKeyDown())return false;
        if(key.getKeyCode()=='[' || key.getKeyCode()==']'){
            scaleSelectionLength(key.getKeyCode()=='['?0.5:2.0);return true;
        }
        if(key.getKeyCode()!=juce::KeyPress::deleteKey && key.getKeyCode()!=juce::KeyPress::backspaceKey)return false;
        return deleteSelection();
    }
private:
    void timerCallback() override {
        finishReleasedGesture(juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown());
    }
    juce::TextButton closeEditor;
    juce::TextButton undoEdit,redoEdit;
    juce::TextButton quantize;
    juce::TextButton transpose;
    juce::Label editFeedback;
    void showEditFeedback(const juce::String& action){
        auto text=action;
        if(session && trackIndex<session->tracks.size() && session->tracks[trackIndex].type==idw::daw::TrackType::midi){
            const auto& notes=session->tracks[trackIndex].notes;
            if(!notes.empty()){
                text+=" | starts: "+juce::String(notes[0].startBeat,3);
                if(notes.size()>1)text+=", "+juce::String(notes[1].startBeat,3);
                if(notes.size()>2)text+=" ...";
                text+=" beats";
            }
        }
        editFeedback.setText(text,juce::dontSendNotification);
    }
    std::uint64_t editGeneration=0;
    void showTransposeMenu(){
        if(dragging || !editingAllowed())return;
        velocity.hideTextBox(false);
        const auto index=trackIndex;const auto generation=editGeneration;
        const auto name=session->tracks[index].name;const auto notes=session->tracks[index].notes;
        juce::PopupMenu menu;
        menu.addItem(1,"Up 1 semitone");menu.addItem(2,"Down 1 semitone");
        menu.addItem(3,"Up 2 semitones");menu.addItem(4,"Down 2 semitones");
        menu.addItem(5,"Up 1 octave (+12)");menu.addItem(6,"Down 1 octave (-12)");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&transpose),
            [safe=juce::Component::SafePointer<IDWPianoRoll>(this),index,generation,name,notes](int result){
                if(result<1 || result>6 || safe==nullptr || !safe->isVisible() || safe->editGeneration!=generation ||
                   safe->dragging || !safe->editingAllowed() || safe->trackIndex!=index)return;
                if(safe->session->tracks[index].name!=name || !idw::daw::sameMidiNotes(safe->session->tracks[index].notes,notes)){
                    if(safe->onHistoryConflict)safe->onHistoryConflict();return;
                }
                constexpr int offsets[]{1,-1,2,-2,12,-12};safe->transposeTrack(offsets[result-1]);
            });
    }
    void showQuantizeMenu(){
        if(dragging || !editingAllowed())return;
        velocity.hideTextBox(false); // Commit pending velocity entry before capturing the menu state.
        const auto index=trackIndex;const auto generation=editGeneration;
        const auto name=session->tracks[index].name;const auto notes=session->tracks[index].notes;
        juce::PopupMenu menu;
        menu.addItem(1,"Quarter notes (1/4)");menu.addItem(2,"Eighth notes (1/8)");
        menu.addItem(4,"Sixteenth notes (1/16)");menu.addItem(8,"Thirty-second notes (1/32)");
        menu.addItem(3,"Eighth-note triplets");menu.addItem(6,"Sixteenth-note triplets");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&quantize),
            [safe=juce::Component::SafePointer<IDWPianoRoll>(this),index,generation,name,notes](int result){
                if(result==0 || safe==nullptr || !safe->isVisible() || safe->editGeneration!=generation ||
                   safe->dragging || !safe->editingAllowed() || safe->trackIndex!=index)return;
                if(safe->session->tracks[index].name!=name || !idw::daw::sameMidiNotes(safe->session->tracks[index].notes,notes)){
                    if(safe->onHistoryConflict)safe->onHistoryConflict();return;
                }
                safe->quantizeTrack(result);
            });
    }
    void restoreFromButton(bool undo){
        if(dragging || !editingAllowed()){showEditFeedback("MIDI history unavailable while busy");return;}
        // Commit an active number editor before applying history. Do not route
        // the keystroke through a text field or the voice command console.
        velocity.hideTextBox(false);
        restoreHistory(undo);
        if(isShowing())grabKeyboardFocus();
    }
    juce::Label velocityLabel;
    juce::Slider velocity;
    juce::TextButton duplicate;
    juce::TextButton halfLength,doubleLength;
    bool velocityDragging=false;
    std::vector<idw::daw::MidiNote> selectedSnapshot;
    std::string selectedTrackName;
    bool selectionMatches() const {
        return selectedValid && session && trackIndex<session->tracks.size() &&
            session->tracks[trackIndex].type==idw::daw::TrackType::midi &&
            session->tracks[trackIndex].name==selectedTrackName &&
            selectedIndex<session->tracks[trackIndex].notes.size() &&
            idw::daw::sameMidiNotes(session->tracks[trackIndex].notes,selectedSnapshot);
    }
    void rememberSelection(){selectedSnapshot=session->tracks[trackIndex].notes;selectedTrackName=session->tracks[trackIndex].name;}
    void revealSelection(){
        if(!selectionMatches())return;
        const auto& n=selectedSnapshot[selectedIndex];
        const double visible=gridWidth()/view.pixelsPerBeat();
        if(n.startBeat<view.firstBeat)view.firstBeat=n.startBeat;
        else if(n.startBeat+std::min(n.lengthBeats,visible)>view.firstBeat+visible)
            view.firstBeat=n.startBeat+std::min(n.lengthBeats,visible)-visible;
        const int rows=idw::daw::PianoRollViewport::rows(gridHeight());
        if(n.pitch>view.topPitch)view.topPitch=n.pitch;
        else if(n.pitch<view.topPitch-rows+1)view.topPitch=n.pitch+rows-1;
        constrainView();
    }
    void refreshVelocity(){
        const bool valid=selectionMatches();
        const bool historyAllowed=editingAllowed() && !dragging;
        undoEdit.setEnabled(historyAllowed && history.canUndo());
        redoEdit.setEnabled(historyAllowed && history.canRedo());
        quantize.setEnabled(historyAllowed && !session->tracks[trackIndex].notes.empty());
        transpose.setEnabled(historyAllowed && !session->tracks[trackIndex].notes.empty());
        velocity.setEnabled(valid && editingAllowed());
        duplicate.setEnabled(valid && editingAllowed() && !dragging);
        halfLength.setEnabled(valid && editingAllowed() && !dragging);
        doubleLength.setEnabled(valid && editingAllowed() && !dragging);
        velocity.setValue(valid?session->tracks[trackIndex].notes[selectedIndex].velocity:100,juce::dontSendNotification);
    }
    idw::daw::PianoRollViewport view;
    int gridWidth() const {return std::max(1,getWidth()-idw::daw::PianoRollViewport::keyWidth);}
    int gridHeight() const {return std::max(13,getHeight()-idw::daw::PianoRollViewport::headerHeight-velocityFooterHeight);}
    double lastBeat() const {
        double end=0;
        if(session && trackIndex<session->tracks.size())for(const auto& n:session->tracks[trackIndex].notes)
            if(std::isfinite(n.startBeat+n.lengthBeats))end=std::max(end,n.startBeat+n.lengthBeats);
        return end;
    }
    void constrainView(){view.constrain(gridWidth(),gridHeight(),lastBeat());}
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
        // Slider change notifications may be asynchronous. Commit its final
        // value while the gesture is still grouped, before the queued callback.
        if(velocityDragging)setSelectionVelocity((int)velocity.getValue());
        if(gestureBefore){
            if(gestureMatches())history.record(*session,gestureTrack,*gestureBefore);
            else if(onHistoryConflict)onHistoryConflict();
        }
        gestureBefore.reset();dragging=false;velocityDragging=false;refreshVelocity();
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
