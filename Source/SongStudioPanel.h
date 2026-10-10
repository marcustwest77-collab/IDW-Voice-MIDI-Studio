#pragma once
#include <JuceHeader.h>
#include "SongStudio.h"

class SongStudioPanel final : public juce::Component,private juce::Timer {
public:
    SongStudioPanel(SongStudio& studio,std::function<void()> close,std::function<void()> enableMic):engine(studio),monitor(std::move(enableMic)){
        title.setText("IDW SONG STUDIO / FIRST WORKFLOW",juce::dontSendNotification);
        info.setText("16 steps per bar / kick, snare, hi-hat. Records one stereo performance track from IDW's vocal/instrument output.\nUse headphones. Record enables microphone monitoring. Save your song before closing IDW. Songs are separate .idwsong files.",juce::dontSendNotification);
        for(auto* c:std::initializer_list<juce::Component*>{&title,&info,&status,&tempo,&bars,&tempoLabel,&barsLabel,&beatGain,&trackGain,&beatLabel,&trackLabel,&play,&record,&stop,&discard,&save,&load,&exportAudio,&done})addAndMakeVisible(c);
        tempoLabel.setText("BPM",juce::dontSendNotification);barsLabel.setText("Bars",juce::dontSendNotification);
        beatLabel.setText("Beat level",juce::dontSendNotification);trackLabel.setText("Recorded track level",juce::dontSendNotification);
        tempo.setRange(40,240,1);bars.setRange(1,64,1);beatGain.setRange(0,2,.01);trackGain.setRange(0,2,.01);
        for(auto* slider:{&tempo,&bars,&beatGain,&trackGain}){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,65,26);slider->onValueChange=[this]{updatePattern();};}
        const char* names[]{"Kick","Snare","Hi-hat"};
        for(int row=0;row<3;++row){rowLabels[(size_t)row].setText(names[row],juce::dontSendNotification);addAndMakeVisible(rowLabels[(size_t)row]);
            for(int step=0;step<16;++step){auto& button=steps[(size_t)(row*16+step)];button.setButtonText(juce::String(step+1));button.setClickingTogglesState(true);button.onClick=[this]{updatePattern();};addAndMakeVisible(button);}}
        play.onClick=[this]{status.setText(engine.start(false)?"Playing from bar 1.":"Could not start; check audio device.",juce::dontSendNotification);};
        record.onClick=[this]{if(engine.hasTake()){status.setText("A take already exists. Save it, then Discard take to record again.",juce::dontSendNotification);return;}monitor();status.setText(engine.start(true)?"Recording. Beat starts immediately at bar 1; no count-in yet.":"Cannot record; check audio device.",juce::dontSendNotification);};
        stop.onClick=[this]{engine.stop();status.setText("Stopped. Save song to preserve your take.",juce::dontSendNotification);};
        discard.onClick=[this]{const juce::Component::SafePointer<SongStudioPanel> safe(this);juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,"Discard recorded take?","This removes the in-memory take. Save the song first if you want to keep it.","Discard","Cancel",this,juce::ModalCallbackFunction::create([safe](int answer){if(safe&&answer){safe->engine.clearTake();safe->status.setText("Take discarded.",juce::dontSendNotification);}}));};
        save.onClick=[this]{choose(0);};load.onClick=[this]{choose(1);};exportAudio.onClick=[this]{choose(2);};done.onClick=std::move(close);
        refresh();startTimerHz(10);
    }
    ~SongStudioPanel() override{stopTimer();}
    void paint(juce::Graphics& g)override{g.fillAll(findColour(juce::ComboBox::backgroundColourId));g.setColour(findColour(juce::Label::textColourId));g.drawRect(getLocalBounds(),2);}
    void resized()override{
        const int w=getWidth();title.setBounds(24,18,w-190,30);done.setBounds(w-140,18,116,30);info.setBounds(24,55,w-48,70);
        tempoLabel.setBounds(24,138,50,30);tempo.setBounds(78,138,250,30);barsLabel.setBounds(355,138,50,30);bars.setBounds(410,138,230,30);
        const int cell=(w-125)/16;
        for(int row=0;row<3;++row){rowLabels[(size_t)row].setBounds(24,192+row*52,74,40);for(int step=0;step<16;++step)steps[(size_t)(row*16+step)].setBounds(104+step*cell,192+row*52,cell-4,40);}
        beatLabel.setBounds(24,365,120,30);beatGain.setBounds(150,365,250,30);trackLabel.setBounds(430,365,175,30);trackGain.setBounds(610,365,250,30);
        play.setBounds(24,425,105,36);record.setBounds(140,425,140,36);stop.setBounds(292,425,100,36);discard.setBounds(410,425,150,36);
        save.setBounds(24,483,160,36);load.setBounds(200,483,160,36);exportAudio.setBounds(376,483,180,36);status.setBounds(24,541,w-48,80);
    }
private:
    void refresh(){const auto p=engine.pattern();tempo.setValue(p.bpm,juce::dontSendNotification);bars.setValue(p.bars,juce::dontSendNotification);beatGain.setValue(p.beatGain,juce::dontSendNotification);trackGain.setValue(p.trackGain,juce::dontSendNotification);for(int r=0;r<3;++r)for(int s=0;s<16;++s)steps[(size_t)(r*16+s)].setToggleState((p.rows[(size_t)r]&(1<<s))!=0,juce::dontSendNotification);}
    void updatePattern(){auto p=engine.pattern();p.bpm=(int)tempo.getValue();p.bars=(int)bars.getValue();p.beatGain=(float)beatGain.getValue();p.trackGain=(float)trackGain.getValue();p.rows.fill(0);for(int r=0;r<3;++r)for(int s=0;s<16;++s)if(steps[(size_t)(r*16+s)].getToggleState())p.rows[(size_t)r]|=(std::uint16_t)(1<<s);if(!engine.setPattern(p)){refresh();status.setText("Maximum song duration is 180 seconds. Reduce bars or increase tempo.",juce::dontSendNotification);}}
    void timerCallback()override{const bool active=engine.isPlaying();for(auto* c:std::initializer_list<juce::Component*>{&tempo,&bars,&beatGain,&trackGain,&play,&record,&discard,&save,&load,&exportAudio})c->setEnabled(!active);for(auto& button:steps)button.setEnabled(!active);if(active)status.setText(juce::String(engine.isRecording()?"Recording ":"Playing ")+juce::String(engine.seconds(),1)+" s — Stop ends transport. Save song afterwards.",juce::dontSendNotification);else if(wasActive)status.setText("Stopped. Take stays in memory until you Save song.",juce::dontSendNotification);wasActive=active;}
    void choose(int action){
        if(engine.isPlaying())return;
        if(action==1){
            const juce::Component::SafePointer<SongStudioPanel> safe(this);
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,"Open another song?","Save your current song first. Opening another replaces its beat and in-memory take.","Open","Cancel",this,juce::ModalCallbackFunction::create([safe](int answer){if(safe&&answer)safe->openChooser(1);}));return;
        }
        openChooser(action);
    }
    void openChooser(int action){
        chooser=std::make_unique<juce::FileChooser>(action==0?"Save IDW song":action==1?"Open IDW song":"Export song mix",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),action==2?"*.wav":"*.idwsong");
        const juce::Component::SafePointer<SongStudioPanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|(action==1?juce::FileBrowserComponent::openMode:juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting),[safe,action](const juce::FileChooser& chosen){
            if(!safe||chosen.getResult()==juce::File{})return;const auto file=action==1?chosen.getResult():chosen.getResult().withFileExtension(action==2?"wav":"idwsong");
            const auto result=action==0?safe->engine.save(file):action==1?safe->engine.load(file):safe->engine.exportWav(file);
            safe->status.setText(result.wasOk()?(action==2?"Exported 48 kHz / 24-bit stereo WAV.":"Song project saved/opened successfully."):result.getErrorMessage(),juce::dontSendNotification);if(result.wasOk())safe->refresh();
        });
    }
    SongStudio& engine;std::function<void()> monitor;bool wasActive=false;
    juce::Label title,info,status,tempoLabel,barsLabel,beatLabel,trackLabel;std::array<juce::Label,3> rowLabels;
    juce::Slider tempo,bars,beatGain,trackGain;std::array<juce::TextButton,48> steps;
    juce::TextButton play{"Play song"},record{"Record take"},stop{"Stop"},discard{"Discard take"},save{"Save song"},load{"Open song"},exportAudio{"Export mix WAV"},done{"Done"};
    std::unique_ptr<juce::FileChooser> chooser;
};
