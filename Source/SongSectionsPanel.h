#pragma once
#include <JuceHeader.h>
#include "SongStudio.h"
class SongSectionsPanel final : public juce::Component,private juce::Timer {
public:
    explicit SongSectionsPanel(SongStudio& studio):engine(studio){
        for(auto* c:std::initializer_list<juce::Component*>{&choice,&name,&first,&last,&nameLabel,&firstLabel,&lastLabel,&add,&apply,&remove,&play,&loop,&stop,&undo,&redo,&help,&message})addAndMakeVisible(c);
        nameLabel.setText("Section name",juce::dontSendNotification);firstLabel.setText("First bar",juce::dontSendNotification);lastLabel.setText("Last bar (included)",juce::dontSendNotification);name.setInputRestrictions(64);name.setText("Verse 1");
        for(auto* slider:{&first,&last}){slider->setRange(1,64,1);slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,60,28);slider->setValue(1,juce::dontSendNotification);}
        choice.setTextWhenNothingSelected("No sections yet");choice.onChange=[this]{showSelected();};
        add.onClick=[this]{const int index=(int)engine.sections().size();const bool ok=engine.setSection(index,draft());refresh(ok?index:-1);report(ok,"Section added. Save song to keep it.");};
        apply.onClick=[this]{const int index=choice.getSelectedId()-1;const bool ok=engine.setSection(index,draft());refresh(index);report(ok,"Section updated. Save song to keep it.");};
        remove.onClick=[this]{const int index=choice.getSelectedId()-1;const juce::Component::SafePointer<SongSectionsPanel> safe(this);juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,"Remove section marker?","This removes the marker only. Audio and lyrics are kept.","Remove","Cancel",this,juce::ModalCallbackFunction::create([safe,index](int confirmed){if(safe&&confirmed){safe->report(safe->engine.removeSection(index),"Section removed. Undo is available.");safe->refresh();}}));};
        play.onClick=[this]{playSelected(false);};loop.onClick=[this]{playSelected(true);};stop.onClick=[this]{engine.stop();message.setText("Stopped. Play song starts the full song from bar 1.",juce::dontSendNotification);};
        undo.onClick=[this]{engine.undoTrackEdit();refresh();};redo.onClick=[this]{engine.redoTrackEdit();refresh();};
        help.setText("Name your Intro, Verse, Hook or Bridge and enter its first and last bar. Add creates a marker; Update changes the selected marker. Play section plays it once; Loop section repeats until Stop.\nSections follow BPM but do not move/stretch audio. Song Bars must include the selected section. Recording still starts at bar 1; export renders the full song once. Loop boundaries do not receive automatic fades.",juce::dontSendNotification);
        refresh();startTimerHz(10);
    }
    void resized()override{const int w=getWidth();choice.setBounds(0,0,w,34);nameLabel.setBounds(0,48,130,30);name.setBounds(140,48,w-140,30);firstLabel.setBounds(0,92,130,30);first.setBounds(140,92,w-140,30);lastLabel.setBounds(0,136,140,30);last.setBounds(140,136,w-140,30);
        add.setBounds(0,184,145,32);apply.setBounds(155,184,145,32);remove.setBounds(310,184,145,32);undo.setBounds(465,184,135,32);redo.setBounds(610,184,135,32);
        play.setBounds(0,230,145,34);loop.setBounds(155,230,145,34);stop.setBounds(310,230,145,34);help.setBounds(0,276,w,90);message.setBounds(0,370,w,48);
    }
private:
    SongStudio::Section draft()const{return {name.getText().trim(),(int)first.getValue(),(int)last.getValue()};}
    void report(bool ok,const juce::String& text){message.setText(ok?text:"Could not edit. Stop transport, select a section and use a name with first/last bars from 1–64 in order (32 sections maximum).",juce::dontSendNotification);}
    void playSelected(bool repeat){const bool ok=engine.playSection(choice.getSelectedId()-1,repeat);message.setText(ok?(repeat?"Looping section. Press Stop to finish.":"Playing section once."):"Stop transport, select a saved section and make sure Song Bars includes its last bar.",juce::dontSendNotification);}
    void showSelected(){const auto sections=engine.sections();const int index=choice.getSelectedId()-1;if(index<0||index>=(int)sections.size())return;const auto& section=sections[(size_t)index];name.setText(section.name,false);first.setValue(section.firstBar,juce::dontSendNotification);last.setValue(section.lastBar,juce::dontSendNotification);}
    void refresh(int select=-1){if(select<0)select=choice.getSelectedId()-1;const auto sections=engine.sections();choice.clear(juce::dontSendNotification);for(int i=0;i<(int)sections.size();++i){const auto& section=sections[(size_t)i];choice.addItem(section.name+" / bars "+juce::String(section.firstBar)+"–"+juce::String(section.lastBar),i+1);}choice.setSelectedId(sections.empty()?0:juce::jlimit(1,(int)sections.size(),select+1),juce::dontSendNotification);showSelected();revision=engine.audioRevision();}
    void timerCallback()override{const bool active=engine.isPlaying(),selected=choice.getSelectedId()>0;for(auto* c:std::initializer_list<juce::Component*>{&choice,&name,&first,&last,&add})c->setEnabled(!active);for(auto* b:{&apply,&remove,&play,&loop})b->setEnabled(!active&&selected);undo.setEnabled(engine.canUndo());redo.setEnabled(engine.canRedo());if(!active&&revision!=engine.audioRevision())refresh();}
    SongStudio& engine;std::uint64_t revision=0;juce::ComboBox choice;juce::TextEditor name;juce::Slider first,last;juce::Label nameLabel,firstLabel,lastLabel,help,message;
    juce::TextButton add{"Add section"},apply{"Update section"},remove{"Remove marker…"},play{"Play section"},loop{"Loop section"},stop{"Stop"},undo{"Undo edit"},redo{"Redo edit"};
};
