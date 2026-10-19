#pragma once
#include <JuceHeader.h>
#include "NativeDictation.h"

class SongDictationPanel final : public juce::Component,private juce::Timer {
public:
    SongDictationPanel(NativeDictation& engine,std::function<double()> rate,std::function<bool(const juce::String&)> append)
        :speech(engine),sampleRate(std::move(rate)),appendText(std::move(append)){
        for(auto* c:std::initializer_list<juce::Component*>{&help,&status,&partial,&draft,&runtime,&model,&start,&stop,&add,&clear})addAndMakeVisible(c);
        help.setText("Speak your lyrics, pause between phrases, then Stop. Review the draft and Append to lyrics. Uses IDW's selected raw microphone input; no second device, audio file or upload.",juce::dontSendNotification);
        draft.setMultiLine(true);draft.setReadOnly(true);draft.setScrollbarsShown(true);draft.setTextToShowWhenEmpty("Recognized phrases appear here. Drafts are temporary until appended and the song is saved.",juce::Colours::grey);
        const auto assets=NativeDictation::installedAssets();
        if(assets!=juce::File{}){
            modelPath=assets.getChildFile("speech-model/vosk-model-small-en-us-0.15");
           #if JUCE_WINDOWS
            runtimePath=assets.getChildFile("vosk/libvosk.dll");
           #elif JUCE_MAC
            runtimePath=assets.getChildFile("vosk/libvosk.dyld");
           #else
            runtimePath=assets.getChildFile("vosk/libvosk.so");
           #endif
        }
        runtime.onClick=[this]{choose(false);};model.onClick=[this]{choose(true);};
        start.onClick=[this]{speech.start(runtimePath,modelPath,sampleRate());update();};
        stop.onClick=[this]{speech.stop();update();};
        add.onClick=[this]{const auto snapshot=speech.snapshot();if(!snapshot.busy&&!snapshot.text.isEmpty()&&appendText(snapshot.text)){speech.clearDraft();update();}};
        clear.onClick=[this]{const juce::Component::SafePointer<SongDictationPanel> safe(this);juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,"Discard dictation draft?","This removes only the temporary speech draft. Appended lyrics are kept.","Discard draft","Cancel",this,juce::ModalCallbackFunction::create([safe](int answer){if(safe&&answer){safe->speech.clearDraft();safe->update();}}));};
        update();startTimerHz(5);
    }
    ~SongDictationPanel() override{stopTimer();speech.stop();}
    void resized()override{
        const int w=getWidth();help.setBounds(0,0,w,60);
        runtime.setBounds(0,70,160,32);model.setBounds(172,70,160,32);start.setBounds(344,70,160,32);stop.setBounds(516,70,145,32);add.setBounds(w-185,70,185,32);
        status.setBounds(0,112,w,44);partial.setBounds(0,158,w-180,28);clear.setBounds(w-170,158,170,28);draft.setBounds(0,196,w,juce::jmax(80,getHeight()-196));
    }
private:
    void update(){
        const auto snapshot=speech.snapshot();status.setText(snapshot.status,juce::dontSendNotification);
        partial.setText(snapshot.partial.isEmpty()?"":("Hearing: "+snapshot.partial),juce::dontSendNotification);
        if(draft.getText()!=snapshot.text)draft.setText(snapshot.text,false);
        start.setEnabled(!snapshot.busy);stop.setEnabled(snapshot.busy);add.setEnabled(!snapshot.busy&&!snapshot.text.isEmpty());clear.setEnabled(!snapshot.busy&&!snapshot.text.isEmpty());runtime.setEnabled(!snapshot.busy);model.setEnabled(!snapshot.busy);
        runtime.setTooltip(runtimePath.getFullPathName());model.setTooltip(modelPath.getFullPathName());
    }
    void timerCallback()override{update();}
    void choose(bool folder){
        chooser=std::make_unique<juce::FileChooser>(folder?"Choose extracted Vosk model folder":"Choose trusted Vosk runtime library",folder?modelPath:runtimePath,folder?"":"*.dll;*.dyld;*.dylib;*.so");
        const juce::Component::SafePointer<SongDictationPanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|(folder?juce::FileBrowserComponent::canSelectDirectories:juce::FileBrowserComponent::canSelectFiles),[safe,folder](const juce::FileChooser& chosen){
            if(!safe||chosen.getResult()==juce::File{})return;
            if(folder)safe->modelPath=chosen.getResult();else safe->runtimePath=chosen.getResult();safe->update();
        });
    }
    NativeDictation& speech;std::function<double()> sampleRate;std::function<bool(const juce::String&)> appendText;
    juce::Label help,status,partial;juce::TextEditor draft;
    juce::TextButton runtime{"Speech runtime…"},model{"Speech model…"},start{"Start dictation"},stop{"Stop dictation"},add{"Append to lyrics"},clear{"Discard draft…"};
    juce::File runtimePath,modelPath;std::unique_ptr<juce::FileChooser> chooser;
};
