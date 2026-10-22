#pragma once
#include <JuceHeader.h>
#include "SongStudio.h"
#include "SongExportJob.h"
#include "SongDictationPanel.h"
#include "SongRecovery.h"
#include "SongTimelinePanel.h"
#include "SongSectionsPanel.h"

class SongStudioPanel final : public juce::Component,private juce::Timer {
public:
    SongStudioPanel(SongStudio& studio,NativeDictation& speech,SongRecovery& checkpointService,std::function<double()> sampleRate,std::function<void()> close,std::function<void()> enableMic):engine(studio),dictation(speech),recovery(checkpointService),monitor(std::move(enableMic)){
        markerPanel=std::make_unique<SongSectionsPanel>(engine);addChildComponent(*markerPanel);addAndMakeVisible(markerTab);markerTab.onClick=[this]{markerVisible=!markerVisible;timelineVisible=false;speechVisible=false;lyricsVisible=true;dictation.stop();resized();};
        timelinePanel=std::make_unique<SongTimelinePanel>(engine,monitor);addChildComponent(*timelinePanel);addAndMakeVisible(timelineTab);timelineTab.onClick=[this]{markerVisible=false;timelineVisible=!timelineVisible;lyricsVisible=true;speechVisible=false;dictation.stop();resized();};
        title.setText("IDW SONG STUDIO",juce::dontSendNotification);
        info.setText("Four audio lanes in Timeline, plus per-bar 16-step drum patterns. Record take captures IDW's processed input into the armed lane (Lead by default). Arm another lane in Timeline.\nUse headphones. Record enables microphone monitoring. Save song keeps your permanent project. Recovery saves completed takes only; in-progress audio is not protected.",juce::dontSendNotification);
        for(auto* c:std::initializer_list<juce::Component*>{&title,&info,&status,&beatBar,&beatTools,&tempo,&bars,&tempoLabel,&barsLabel,&beatGain,&trackGain,&beatLabel,&trackLabel,&play,&record,&stop,&discard,&save,&load,&exportAudio,&done})addAndMakeVisible(c);
        beatBar.onChange=[this]{refresh();};beatTools.onClick=[this]{showBeatTools();};
        beatBar.setTooltip("Base repeats on bars without a custom pattern. Select a numbered bar to edit that bar only. * means custom.");
        tempoLabel.setText("BPM",juce::dontSendNotification);barsLabel.setText("Bars",juce::dontSendNotification);
        beatLabel.setText("Beat level",juce::dontSendNotification);trackLabel.setText("Audio tracks level",juce::dontSendNotification);
        tempo.setRange(40,240,1);bars.setRange(1,64,1);beatGain.setRange(0,2,.01);trackGain.setRange(0,2,.01);
        for(auto* slider:{&tempo,&bars,&beatGain,&trackGain}){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,65,26);slider->onValueChange=[this]{auto p=engine.pattern();p.bpm=(int)tempo.getValue();p.bars=(int)bars.getValue();p.beatGain=(float)beatGain.getValue();p.trackGain=(float)trackGain.getValue();if(!engine.setPattern(p))status.setText("Maximum duration is 180 seconds; reduce Bars or increase BPM.",juce::dontSendNotification);refresh();};}
        const char* names[]{"Kick","Snare","Hi-hat"};
        for(int row=0;row<3;++row){rowLabels[(size_t)row].setText(names[row],juce::dontSendNotification);addAndMakeVisible(rowLabels[(size_t)row]);
            for(int step=0;step<16;++step){auto& button=steps[(size_t)(row*16+step)];button.setButtonText(juce::String(step+1));button.setClickingTogglesState(true);button.onClick=[this]{updatePattern();};addAndMakeVisible(button);}}
        play.onClick=[this]{status.setText(engine.start(false)?"Playing from bar 1.":"Could not start; check audio device.",juce::dontSendNotification);};
        record.onClick=[this]{if(engine.armedLaneHasAudio()){status.setText("The armed lane already has audio. Use Timeline > Record cursor to add a new clip, or arm an empty lane for bar-1 recording.",juce::dontSendNotification);return;}monitor();status.setText(engine.start(true)?(engine.isCountingIn()?"Count-in started. Get ready for bar 1.":"Recording from bar 1."):"Cannot record; check audio device.",juce::dontSendNotification);};
        stop.onClick=[this]{const bool cancelled=engine.isCountingIn();engine.stop();status.setText(cancelled?"Count-in cancelled. No take recorded.":"Stopped. Save song to preserve your take.",juce::dontSendNotification);};
        discard.onClick=[this]{const int lane=engine.armedLane();const juce::Component::SafePointer<SongStudioPanel> safe(this);juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,"Clear armed track?","Remove every clip from "+juce::String(SongStudio::laneName(lane))+"? Save song first to keep a permanent copy.","Discard","Cancel",this,juce::ModalCallbackFunction::create([safe,lane](int answer){if(safe&&answer&&safe->engine.removeLane(lane))safe->status.setText("Armed track cleared.",juce::dontSendNotification);}));};
        save.onClick=[this]{choose(0);};load.onClick=[this]{choose(1);};exportAudio.onClick=[this]{if(exportJob){exportJob->cancel();return;}juce::PopupMenu menu;menu.addItem(1,"Export full mix WAV");menu.addItem(2,"Export separate track WAVs");menu.addItem(3,"Check full-song levels");const juce::Component::SafePointer<SongStudioPanel> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportAudio),[safe](int choice){if(!safe)return;if(choice==1)safe->choose(2);else if(choice==2)safe->chooseStems();else if(choice==3)safe->startExport({},false,true);});};done.onClick=[this,close=std::move(close)]{dictation.stop();close();};
        speechPanel=std::make_unique<SongDictationPanel>(dictation,std::move(sampleRate),[this](const juce::String& text){const auto combined=lyricText.getText()+(lyricText.isEmpty()?"":"\n")+text;if(!engine.setLyrics({songTitle.getText(),combined})){status.setText("Lyrics full. Copy the dictation draft before closing; existing lyrics kept.",juce::dontSendNotification);return false;}lyricText.setText(combined,false);status.setText("Dictation appended. Save song to keep your lyrics.",juce::dontSendNotification);return true;});
        addChildComponent(*speechPanel);addAndMakeVisible(speechTab);speechTab.onClick=[this]{markerVisible=false;timelineVisible=false;speechVisible=!speechVisible;lyricsVisible=true;if(!speechVisible)dictation.stop();resized();};
        for(auto* c:std::initializer_list<juce::Component*>{&lyricsTab,&songTitle,&lyricText,&lyricsHelp,&importLyrics,&exportLyrics})addAndMakeVisible(c);
        songTitle.setTextToShowWhenEmpty("Song title",juce::Colours::grey);songTitle.setInputRestrictions(256);
        lyricText.setMultiLine(true);lyricText.setReturnKeyStartsNewLine(true);lyricText.setScrollbarsShown(true);lyricText.setInputRestrictions(32768);
        lyricText.setTextToShowWhenEmpty("Write or paste your lyrics here. Save song stores them with your beat and take.",juce::Colours::grey);
        songTitle.onTextChange=[this]{syncLyrics();};lyricText.onTextChange=[this]{syncLyrics();};
        lyricsHelp.setText("Lyrics stay with this song. Paste text or Import TXT from Audio Lab. Open Dictation to speak a draft. Save song before closing.",juce::dontSendNotification);
        lyricsTab.onClick=[this]{if(markerVisible){markerVisible=false;lyricsVisible=true;}else if(timelineVisible){timelineVisible=false;lyricsVisible=true;}else if(speechVisible){speechVisible=false;lyricsVisible=true;dictation.stop();}else lyricsVisible=!lyricsVisible;resized();};
        const char* headings[]{"Intro","Verse","Hook","Bridge","Outro"};
        for(size_t i=0;i<sections.size();++i){sections[i].setButtonText(headings[i]);addAndMakeVisible(sections[i]);sections[i].onClick=[this,i]{lyricText.insertTextAtCaret("\n["+sections[i].getButtonText()+"]\n");syncLyrics();};}
        importLyrics.onClick=[this]{openChooser(3);};exportLyrics.onClick=[this]{openChooser(4);};
        addAndMakeVisible(recover);addAndMakeVisible(recoveryStatus);recover.onClick=[this]{showRecovery();};
        for(auto* c:std::initializer_list<juce::Component*>{&countInChoice,&clickToggle,&clickLevel,&clickLabel})addAndMakeVisible(c);
        countInChoice.addItem("Count-in: Off",1);countInChoice.addItem("Count-in: 1 bar",2);countInChoice.addItem("Count-in: 2 bars",3);
        countInChoice.setSelectedId(engine.countInBars()+1,juce::dontSendNotification);
        countInChoice.onChange=[this]{if(!engine.setCountInBars(countInChoice.getSelectedId()-1))countInChoice.setSelectedId(engine.countInBars()+1,juce::dontSendNotification);};
        countInChoice.setTooltip("Recording count-in in 4/4. Session setting; not stored in song files. Playback starts immediately.");
        clickToggle.setToggleState(engine.metronomeEnabled(),juce::dontSendNotification);clickToggle.onClick=[this]{engine.setMetronomeEnabled(clickToggle.getToggleState());};
        clickToggle.setTooltip("Metronome during recording/playback. Clicks are excluded from the recorded track and WAV export; use headphones to prevent microphone bleed.");
        clickLabel.setText("Click level",juce::dontSendNotification);clickLevel.setSliderStyle(juce::Slider::LinearHorizontal);clickLevel.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,26);clickLevel.setRange(0,.5,.01);clickLevel.setValue(engine.metronomeGain(),juce::dontSendNotification);
        clickLevel.onValueChange=[this]{engine.setMetronomeGain((float)clickLevel.getValue());};
        refresh();refreshLyrics();startTimerHz(10);
    }
    ~SongStudioPanel() override{stopTimer();dictation.stop();recovery.requestCheckpoint();}
    void visibilityChanged() override{if(!isShowing())dictation.stop();}
    void paint(juce::Graphics& g)override{g.fillAll(findColour(juce::ComboBox::backgroundColourId));g.setColour(findColour(juce::Label::textColourId));g.drawRect(getLocalBounds(),2);}
    void resized()override{
        const int w=getWidth();title.setBounds(24,18,w-780,30);
        markerTab.setBounds(w-655,18,100,30);markerPanel->setBounds(24,135,w-48,getHeight()-330);markerPanel->setVisible(markerVisible);timelineTab.setBounds(w-760,18,95,30);timelinePanel->setBounds(24,135,w-48,getHeight()-330);timelinePanel->setVisible(timelineVisible);
        recover.setBounds(w-545,18,100,30);recoveryStatus.setBounds(24,getHeight()-35,w-48,24);
        speechTab.setBounds(w-435,18,115,30);speechTab.setButtonText(speechVisible?"Back to lyrics":"Dictation");
        speechPanel->setBounds(24,135,w-48,getHeight()-330);speechPanel->setVisible(speechVisible);
        lyricsTab.setBounds(w-310,18,150,30);lyricsTab.setButtonText(lyricsVisible&&!speechVisible&&!timelineVisible&&!markerVisible?"Beat controls":"Lyrics");
        for(auto* c:std::initializer_list<juce::Component*>{&beatBar,&beatTools,&tempo,&bars,&tempoLabel,&barsLabel,&beatGain,&trackGain,&beatLabel,&trackLabel,&discard})c->setVisible(!lyricsVisible);
        for(auto& label:rowLabels)label.setVisible(!lyricsVisible);for(auto& step:steps)step.setVisible(!lyricsVisible);
        for(auto* c:std::initializer_list<juce::Component*>{&songTitle,&lyricText,&lyricsHelp,&importLyrics,&exportLyrics})c->setVisible(lyricsVisible&&!speechVisible&&!timelineVisible&&!markerVisible);
        songTitle.setBounds(24,135,w-48,30);lyricsHelp.setBounds(24,171,w-48,36);
        for(size_t i=0;i<sections.size();++i){sections[i].setVisible(lyricsVisible&&!speechVisible&&!timelineVisible&&!markerVisible);sections[i].setBounds(24+(int)i*92,213,84,28);}
        importLyrics.setBounds(w-284,213,120,28);exportLyrics.setBounds(w-154,213,130,28);
        lyricText.setBounds(24,250,w-48,juce::jmax(160,getHeight()-435));done.setBounds(w-140,18,116,30);info.setBounds(24,55,w-48,70);
        tempoLabel.setBounds(24,138,50,30);tempo.setBounds(78,138,250,30);barsLabel.setBounds(355,138,50,30);bars.setBounds(410,138,230,30);
        beatBar.setBounds(650,138,170,30);beatTools.setBounds(830,138,140,30);
        const int cell=(w-125)/16;
        for(int row=0;row<3;++row){rowLabels[(size_t)row].setBounds(24,192+row*52,74,40);for(int step=0;step<16;++step)steps[(size_t)(row*16+step)].setBounds(104+step*cell,192+row*52,cell-4,40);}
        beatLabel.setBounds(24,365,120,30);beatGain.setBounds(150,365,250,30);trackLabel.setBounds(430,365,175,30);trackGain.setBounds(610,365,250,30);
        play.setBounds(24,425,105,36);record.setBounds(140,425,140,36);stop.setBounds(292,425,100,36);discard.setBounds(410,425,150,36);
        save.setBounds(24,483,160,36);load.setBounds(200,483,160,36);exportAudio.setBounds(376,483,180,36);status.setBounds(24,541,w-48,80);
        const int transportY=lyricsVisible?getHeight()-175:425;
        countInChoice.setBounds(lyricsVisible?410:580,transportY,180,36);clickToggle.setBounds(lyricsVisible?602:772,transportY,175,36);
        const int fileRowY=lyricsVisible?transportY+48:483;clickLabel.setBounds(590,fileRowY,90,36);clickLevel.setBounds(685,fileRowY,w-709,36);
        if(lyricsVisible){const int y=getHeight()-175;play.setBounds(24,y,105,36);record.setBounds(140,y,140,36);stop.setBounds(292,y,100,36);save.setBounds(24,y+48,160,36);load.setBounds(200,y+48,160,36);exportAudio.setBounds(376,y+48,180,36);status.setBounds(24,y+92,w-48,44);}
    }
private:
    int editedBar()const{return beatBar.getSelectedId()-2;}
    void showBeatTools(){
        if(engine.isPlaying())return;juce::PopupMenu menu;menu.addItem(1,"Copy displayed pattern to bars…");menu.addItem(2,"Use Base on selected bar",editedBar()>=0);menu.addItem(3,"Undo beat/track edit",engine.canUndo());menu.addItem(4,"Redo beat/track edit",engine.canRedo());
        const juce::Component::SafePointer<SongStudioPanel> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&beatTools),[safe](int choice){if(!safe||safe->engine.isPlaying())return;
            if(choice==3){safe->engine.undoTrackEdit();safe->refresh();return;}if(choice==4){safe->engine.redoTrackEdit();safe->refresh();return;}
            const int bar=safe->editedBar();auto pattern=safe->engine.pattern();
            if(choice==2&&bar>=0){safe->engine.useBaseBeat(bar);safe->refresh();return;}
            if(choice!=1)return;const auto source=pattern.rowsAtBar(bar);const auto revision=safe->engine.audioRevision();
            auto* dialog=new juce::AlertWindow("Copy beat to bars","Replaces patterns on the inclusive bar range. Undo restores the previous beats.",juce::MessageBoxIconType::QuestionIcon);
            dialog->addTextEditor("first","1","First bar");dialog->addTextEditor("last",juce::String(pattern.bars),"Last bar");dialog->getTextEditor("first")->setInputRestrictions(2,"0123456789");dialog->getTextEditor("last")->setInputRestrictions(2,"0123456789");dialog->addButton("Copy",1);dialog->addButton("Cancel",0);
            const juce::Component::SafePointer<juce::AlertWindow> alert(dialog);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,alert,source,revision](int answer){if(!safe||!alert||answer!=1)return;auto next=safe->engine.pattern();const auto first=alert->getTextEditorContents("first").trim(),last=alert->getTextEditorContents("last").trim();
                const int a=first.getIntValue(),b=last.getIntValue();if(first.length()>2||last.length()>2||!first.containsOnly("0123456789")||!last.containsOnly("0123456789")||a<1||b<a||b>next.bars||safe->engine.audioRevision()!=revision){safe->status.setText("Copy rejected: enter a range within song Bars, and retry if the song changed.",juce::dontSendNotification);return;}
                const bool ok=safe->engine.copyBeatRange(source,a-1,b-1,revision);safe->status.setText(ok?"Beat copied. Save song to keep the arrangement.":"Copy rejected: stop transport and retry if the song changed.",juce::dontSendNotification);safe->refresh();
            }),true);
        });
    }
    void showRecovery(){
        if(engine.isPlaying()||dictation.busy()){status.setText("Stop playback, recording and dictation before recovering.",juce::dontSendNotification);return;}
        const auto entries=recovery.entries();
        if(entries.empty()){status.setText("No recovery checkpoints yet. Writing saves every 10 seconds; stopped songs every 30 seconds.",juce::dontSendNotification);return;}
        auto* dialog=new juce::AlertWindow("Recover saved work","Song restores the last stopped beat/take checkpoint. Writing may be newer: restore it after the song. Current work is replaced only after confirmation.",juce::MessageBoxIconType::QuestionIcon);
        juce::StringArray names;for(const auto& entry:entries)names.add(entry.label);
        dialog->addComboBox("checkpoint",names,"Checkpoint");dialog->addButton("Restore song",1);dialog->addButton("Restore writing",2);dialog->addButton("Open folder",3);dialog->addButton("Cancel",0);
        const juce::Component::SafePointer<SongStudioPanel> safe(this);
        const juce::Component::SafePointer<juce::AlertWindow> alert(dialog);
        dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,alert,entries](int action){
            if(!safe||!alert||action==0)return;
            if(action==3){safe->recovery.directory().revealToUser();return;}
            const int selected=alert->getComboBoxComponent("checkpoint")->getSelectedItemIndex();
            if(selected<0||selected>=(int)entries.size())return;
            const auto folder=entries[(size_t)selected].folder;
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,"Replace current work?",action==1?"Save your current song first. This replaces the beat, take and lyrics. Append or discard any dictation draft before restoring.":"This replaces your title, lyrics and dictation draft. Your beat and take are kept. Save any current writing first.","Restore","Cancel",safe.getComponent(),juce::ModalCallbackFunction::create([safe,folder,action](int confirmed){
                if(!safe||!confirmed)return;bool previous=false;
                const auto result=action==1?safe->recovery.restoreSong(folder,previous):safe->recovery.restoreWriting(folder,previous);
                if(result.wasOk()){safe->refresh();safe->refreshLyrics();}
                safe->status.setText(result.failed()?result.getErrorMessage():previous?"Recovered previous checkpoint (newest was unreadable). Review and Save song.":"Checkpoint recovered. Review and Save song; writing may be newer than the song checkpoint.",juce::dontSendNotification);
            }));
        }),true);
    }
    void syncLyrics(){if(!engine.setLyrics({songTitle.getText(),lyricText.getText()}))status.setText("Lyrics exceed the project size limit. Shorten the text before saving.",juce::dontSendNotification);}
    void refreshLyrics(){const auto doc=engine.lyrics();songTitle.setText(doc.title,false);lyricText.setText(doc.text,false);}
    void refresh(){const auto p=engine.pattern();const int selected=juce::jlimit(1,p.bars+1,juce::jmax(1,beatBar.getSelectedId()));beatBar.clear(juce::dontSendNotification);beatBar.addItem("Edit: Base pattern",1);for(int bar=0;bar<p.bars;++bar)beatBar.addItem("Edit: Bar "+juce::String(bar+1)+(p.customBars[(size_t)bar]?" *":""),bar+2);beatBar.setSelectedId(selected,juce::dontSendNotification);const auto& displayed=p.rowsAtBar(editedBar());beatRevision=engine.audioRevision();tempo.setValue(p.bpm,juce::dontSendNotification);bars.setValue(p.bars,juce::dontSendNotification);beatGain.setValue(p.beatGain,juce::dontSendNotification);trackGain.setValue(p.trackGain,juce::dontSendNotification);for(int r=0;r<3;++r)for(int s=0;s<16;++s)steps[(size_t)(r*16+s)].setToggleState((displayed[(size_t)r]&(1<<s))!=0,juce::dontSendNotification);}
    void updatePattern(){auto p=engine.pattern();p.bpm=(int)tempo.getValue();p.bars=(int)bars.getValue();p.beatGain=(float)beatGain.getValue();p.trackGain=(float)trackGain.getValue();auto& rows=editedBar()<0?p.rows:p.barRows[(size_t)editedBar()];rows.fill(0);for(int r=0;r<3;++r)for(int s=0;s<16;++s)if(steps[(size_t)(r*16+s)].getToggleState())rows[(size_t)r]|=(std::uint16_t)(1<<s);if(editedBar()>=0)p.customBars[(size_t)editedBar()]=true;if(!engine.setPattern(p)){refresh();status.setText("Maximum song duration is 180 seconds. Reduce bars or increase tempo.",juce::dontSendNotification);}else refresh();}
    void timerCallback()override{record.setButtonText("Record lane "+juce::String(engine.armedLane()+1));record.setTooltip("Record into "+juce::String(SongStudio::laneName(engine.armedLane())));recoveryStatus.setText(recovery.status(),juce::dontSendNotification);const bool active=engine.isPlaying();beatBar.setEnabled(!active);beatTools.setEnabled(!active);if(!active&&beatRevision!=engine.audioRevision())refresh();countInChoice.setEnabled(!active);for(auto* c:std::initializer_list<juce::Component*>{&tempo,&bars,&beatGain,&trackGain,&play,&record,&discard,&save,&load,&exportAudio})c->setEnabled(!active);for(auto& button:steps)button.setEnabled(!active);if(active)status.setText(engine.isCountingIn()?("Count-in "+juce::String(engine.countInBeat())+" / "+juce::String(engine.countInTotalBeats())+" — get ready; no audio is recorded yet."):((engine.isRecording()?"Recording "+juce::String(SongStudio::laneName(engine.armedLane()))+" / ":juce::String(engine.isLooping()?"Looping ":"Playing "))+"bar "+juce::String(engine.songBar())+", beat "+juce::String(engine.songBeat())+" / "+juce::String(engine.seconds(),1)+" s — Save song afterwards."),juce::dontSendNotification);else if(wasActive)status.setText("Stopped. Take stays in memory until you Save song.",juce::dontSendNotification);wasActive=active;updateExport();}
    void choose(int action){
        if(engine.isPlaying())return;
        if(action==1&&(dictation.busy()||!dictation.snapshot().text.isEmpty())){status.setText("Stop dictation and append or discard your draft before opening another song.",juce::dontSendNotification);return;}
        if(action==1){
            const juce::Component::SafePointer<SongStudioPanel> safe(this);
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon,"Open another song?","Save your current song first. Opening another replaces its lyrics, beat and in-memory take.","Open","Cancel",this,juce::ModalCallbackFunction::create([safe](int answer){if(safe&&answer)safe->openChooser(1);}));return;
        }
        openChooser(action);
    }
    void startExport(const juce::File& destination,bool stems,bool checkLevels=false){
        if(exportJob){status.setText("An export is already running.",juce::dontSendNotification);return;}
        std::uint64_t revision=0;auto snapshot=engine.recoverySnapshot(revision);
        if(!snapshot){status.setText("Stop transport before starting an export.",juce::dontSendNotification);return;}
        exportJob=std::make_unique<SongExportJob>(std::move(snapshot),destination,stems,checkLevels);
        if(!exportJob->start()){exportJob.reset();status.setText("Could not start export worker.",juce::dontSendNotification);return;}
        updateExport();
    }
    void updateExport(){
        if(!exportJob){exportAudio.setButtonText("Export audio…");return;}
        exportAudio.setEnabled(true);exportAudio.setButtonText(exportJob->levelCheck?"Cancel check":"Cancel export");
        if(!exportJob->finished()){status.setText(exportJob->cancellationRequested()?(exportJob->levelCheck?"Cancelling level check…":"Cancelling export and cleaning up partial files…"):juce::String(exportJob->levelCheck?"Checking song snapshot / ":"Exporting song snapshot / ")+juce::String((int)(100*exportJob->fraction()))+"%. You can keep editing; changes apply to the next export.",juce::dontSendNotification);return;}
        const auto result=exportJob->result();if(exportJob->levelCheck&&result.wasOk()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Full-song level check",exportJob->levelSummary(),"OK",this);
            status.setText("Level check complete for the snapshot. Scan again after changing the mix.",juce::dontSendNotification);
        }else status.setText(result.wasOk()?"Export complete: "+exportJob->destination.getFullPathName():result.getErrorMessage(),juce::dontSendNotification);
        exportJob.reset();exportAudio.setButtonText("Export audio…");exportAudio.setEnabled(!engine.isPlaying());
    }
    void chooseStems(){
        if(engine.isPlaying())return;
        chooser=std::make_unique<juce::FileChooser>("Choose parent folder for separate track WAVs",juce::File::getSpecialLocation(juce::File::userMusicDirectory));
        const juce::Component::SafePointer<SongStudioPanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](const juce::FileChooser& chosen){
            if(!safe||chosen.getResult()==juce::File{})return;
            const auto folder=chosen.getResult().getChildFile("IDW-Stems-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S")+"-"+juce::Uuid().toString().substring(0,8));
            safe->startExport(folder,true);
        });
    }
    void openChooser(int action){
        chooser=std::make_unique<juce::FileChooser>(action==0?"Save IDW song":action==1?"Open IDW song":action==2?"Export song mix":action==3?"Import lyrics (append)":"Export lyrics",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),action>=3?"*.txt":action==2?"*.wav":"*.idwsong");
        const juce::Component::SafePointer<SongStudioPanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::canSelectFiles|((action==1||action==3)?juce::FileBrowserComponent::openMode:juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting),[safe,action](const juce::FileChooser& chosen){
            if(!safe||chosen.getResult()==juce::File{})return;const auto file=(action==1||action==3)?chosen.getResult():chosen.getResult().withFileExtension(action==4?"txt":action==2?"wav":"idwsong");
            if(action>=3){
                if(action==3){
                    if(file.getSize()>SongStudio::maxLyricsBytes){safe->status.setText("Text file too large. Existing lyrics kept.",juce::dontSendNotification);return;}
                    juce::MemoryBlock bytes;if(!file.loadFileAsData(bytes)){safe->status.setText("Cannot read lyrics file.",juce::dontSendNotification);return;}
                    const auto* data=static_cast<const char*>(bytes.getData());
                    if(bytes.getSize()>0&&(std::memchr(data,0,bytes.getSize())!=nullptr||!juce::CharPointer_UTF8::isValidString(data,(int)bytes.getSize()))){safe->status.setText("Use a UTF-8 text file. Existing lyrics kept.",juce::dontSendNotification);return;}
                    const auto addition=bytes.getSize()>0?juce::String::fromUTF8(data,(int)bytes.getSize()):juce::String{};
                    const auto combined=safe->lyricText.getText()+(safe->lyricText.isEmpty()?"":"\n")+addition;
                    if(combined.length()>32768||combined.getNumBytesAsUTF8()>SongStudio::maxLyricsBytes){safe->status.setText("Combined lyrics too long. Existing lyrics kept.",juce::dontSendNotification);return;}
                    safe->lyricText.setText(combined,false);safe->syncLyrics();safe->status.setText("Lyrics appended. Save song to keep them with this project.",juce::dontSendNotification);
                }else{
                    juce::TemporaryFile temp(file);const bool ok=temp.getFile().replaceWithText(safe->lyricText.getText())&&temp.overwriteTargetFileWithTemporary();safe->status.setText(ok?"Lyrics exported as text.":"Lyrics export failed.",juce::dontSendNotification);
                }
                return;
            }
            if(action==0)safe->syncLyrics();
            if(action==2){safe->startExport(file,false);return;}
            const auto result=action==0?safe->engine.save(file):safe->engine.load(file);
            safe->status.setText(result.wasOk()?(action==2?"Exported 48 kHz / 24-bit stereo WAV.":"Song project saved/opened successfully."):result.getErrorMessage(),juce::dontSendNotification);if(result.wasOk()){safe->refresh();if(action==1)safe->refreshLyrics();}
        });
    }
    std::unique_ptr<SongSectionsPanel> markerPanel;juce::TextButton markerTab{"Sections"};bool markerVisible=false;
    std::unique_ptr<SongTimelinePanel> timelinePanel;juce::TextButton timelineTab{"Timeline"};bool timelineVisible=false;
    juce::ComboBox countInChoice;juce::ToggleButton clickToggle{"Metronome"};juce::Slider clickLevel;juce::Label clickLabel;
    juce::TextButton recover{"Recover…"};juce::Label recoveryStatus;
    bool lyricsVisible=false,speechVisible=false;
    std::unique_ptr<SongDictationPanel> speechPanel;juce::TextButton speechTab{"Dictation"};
    juce::TextButton lyricsTab{"Lyrics"},importLyrics{"Import TXT"},exportLyrics{"Export TXT"};
    juce::TextEditor songTitle,lyricText;juce::Label lyricsHelp;std::array<juce::TextButton,5> sections;
    SongStudio& engine;NativeDictation& dictation;SongRecovery& recovery;std::function<void()> monitor;bool wasActive=false;
    juce::Label title,info,status,tempoLabel,barsLabel,beatLabel,trackLabel;std::array<juce::Label,3> rowLabels;
    juce::Slider tempo,bars,beatGain,trackGain;std::array<juce::TextButton,48> steps;
    juce::TextButton play{"Play song"},record{"Record take"},stop{"Stop"},discard{"Clear armed track"},save{"Save song"},load{"Open song"},exportAudio{"Export audio…"},done{"Done"};
    juce::ComboBox beatBar;juce::TextButton beatTools{"Beat tools…"};std::uint64_t beatRevision=0;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<SongExportJob> exportJob;
};
