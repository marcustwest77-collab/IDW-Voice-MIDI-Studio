#include "PluginEditor.h"
#include "MidiExport.h"
#include "SetupDiagnostics.h"
#include "VoiceProfiles.h"
#include <cmath>
namespace {
const juce::Colour background{0xff0b0e14},card{0xff141a24},gold{0xffeac36c},muted{0xff9aabc2},mint{0xff68dfbd};
const char* names[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char* drumIDs[]={"kick","snare","hat","pad4","pad5","pad6","pad7","pad8"};
}
IDWVoiceMIDIStudioAudioProcessorEditor::IDWVoiceMIDIStudioAudioProcessorEditor(IDWVoiceMIDIStudioAudioProcessor& processor)
 : AudioProcessorEditor(&processor),p(processor){
    theme.setColour(juce::ResizableWindow::backgroundColourId,background);
    theme.setColour(juce::Slider::rotarySliderFillColourId,gold);
    theme.setColour(juce::Slider::thumbColourId,gold);
    theme.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    theme.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff263144));
    theme.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff76602d));
    theme.setColour(juce::ComboBox::backgroundColourId,card);
    theme.setColour(juce::ToggleButton::tickColourId,mint);
    setLookAndFeel(&theme);setResizable(true,true);setResizeLimits(1040,890,1600,1200);setSize(1120,900);
    title.setText("IDW / VOICE MIDI STUDIO",juce::dontSendNotification);title.setFont(juce::FontOptions(25.0f,juce::Font::bold));
    readout.setFont(juce::FontOptions(31.0f,juce::Font::bold));readout.setColour(juce::Label::textColourId,mint);
    traceLabel.setText("LIVE PITCH  /  7 SECONDS",juce::dontSendNotification);traceLabel.setColour(juce::Label::textColourId,muted);
    status.setText("Start with Preview sound, then sing or press Test note.",juce::dontSendNotification);
    diagnostics.setColour(juce::Label::textColourId,muted);
    drumLabel.setText("DRUM LAB  /  Train each pad with five distinct hits",juce::dontSendNotification);drumLabel.setColour(juce::Label::textColourId,gold);
    for(auto* c:std::initializer_list<juce::Component*>{&setupStatus,&copyDiagnostics,&title,&readout,&status,&diagnostics,&traceLabel,&drumLabel,&captureLabel,&rootSelector,&expressionSelector,&inputSelector,&presetSelector,&learnTarget,&trainTarget,&scaleLock,&beatbox,&melody,&preview,&monitor,&mpeToggle,&calibrate,&panic,&test,&help,&save,&learn,&clearLearn,&record,&exportMidi,&train,&cancelTrain,&clearTrain})addAndMakeVisible(c);
    configureSlider(gateSlider,gateLabel,"NOISE GATE","Minimum input level. Calibrate while quiet.");
    configureSlider(confidenceSlider,confidenceLabel,"CONFIDENCE","Pitch certainty required to play a note.");
    configureSlider(bendSlider,bendLabel,"BEND RANGE","Match this semitone range in your synth if it ignores MIDI RPN.");
    configureSlider(tuneSlider,tuneLabel,"TUNING / CENTS","Fine tuning offset; normally leave at zero.");
    auto attachSlider=[this](const char* id,juce::Slider& s){sliders.push_back(std::make_unique<SliderAttachment>(p.apvts,id,s));};
    attachSlider("gate",gateSlider);attachSlider("confidence",confidenceSlider);attachSlider("bend",bendSlider);attachSlider("tuneCents",tuneSlider);
    gateSlider.textFromValueFunction=[](double v){return juce::String(v,3);};
    tuneSlider.textFromValueFunction=[](double v){return juce::String(v,0);};
    gateSlider.setNumDecimalPlacesToDisplay(3);confidenceSlider.setNumDecimalPlacesToDisplay(2);bendSlider.setNumDecimalPlacesToDisplay(0);tuneSlider.setNumDecimalPlacesToDisplay(0);
    for(int i=0;i<12;++i)rootSelector.addItem(names[i],i+1);
    expressionSelector.addItem("Strict scale",1);expressionSelector.addItem("Natural vibrato",2);
    expressionSelector.setTooltip("Scale lock off: free pitch. Strict: centered pitch. Natural: retains up to 45 cents of deviation around the selected scale note.");
    inputSelector.addItem("Input: Left / mono",1);inputSelector.addItem("Input: Right",2);inputSelector.addItem("Input: L + R",3);
    for(auto pair:{std::pair<const char*,juce::ComboBox*>{"root",&rootSelector},{"scaleExpression",&expressionSelector},{"inputMode",&inputSelector}})
        combos.push_back(std::make_unique<ComboAttachment>(p.apvts,pair.first,*pair.second));
    for(auto pair:{std::pair<const char*,juce::ToggleButton*>{"scaleLock",&scaleLock},{"beatbox",&beatbox},{"melody",&melody},{"previewAudio",&preview},{"monitorMic",&monitor},{"mpe",&mpeToggle}})
        buttons.push_back(std::make_unique<ButtonAttachment>(p.apvts,pair.first,*pair.second));
    for(int i=0;i<12;++i){addAndMakeVisible(scaleButtons[i]);scaleButtons[i].onClick=[this,i]{
        const int mask=(int)p.apvts.getRawParameterValue("scaleMask")->load(),next=mask^(1<<i);
        if(next==0){status.setText("Keep at least one scale note enabled.",juce::dontSendNotification);return;}
        auto* param=p.apvts.getParameter("scaleMask");param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1((float)next));param->endChangeGesture();syncScale();};}
    const juce::StringArray learnLabels{"Noise gate","Confidence","Bend range","Hit threshold","Gesture sensitivity","Scale expression"};for(int i=0;i<learnLabels.size();++i)learnTarget.addItem(learnLabels[i],i+1);learnTarget.setSelectedId(1);
    learn.onClick=[this]{p.learn.arm(p.learn.destinations()[learnTarget.getSelectedId()-1]);status.setText("Move a hardware MIDI controller to map it.",juce::dontSendNotification);};
    clearLearn.onClick=[this]{p.learn.clear();status.setText("MIDI controller mappings cleared.",juce::dontSendNotification);};
    presetSelector.onChange=[this]{loadPreset();};
    save.onClick=[this]{p.saveExtraState();const auto name="IDW V4 "+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
        if(p.presets.save(name)){refreshPresets();status.setText("Saved settings, controller mappings and drum profiles.",juce::dontSendNotification);}else status.setText("Could not save preset.",juce::dontSendNotification);};
    copyDiagnostics.onClick=[this]{juce::SystemClipboard::copyTextToClipboard(diagnosticReport());status.setText("Diagnostics copied. Paste them into your support chat.",juce::dontSendNotification);};
    setupStatus.setColour(juce::Label::textColourId,gold);
    setupStatus.setFont(juce::FontOptions(14.0f));
    calibrate.onClick=[this]{if(!audioRunning()){status.setText("Start audio processing before calibrating.",juce::dontSendNotification);return;}calibrationPeak=0;noisePeak=0;calibrating=true;calibrationStarted=juce::Time::getMillisecondCounterHiRes();calibrate.setEnabled(false);status.setText("Stay quiet: measuring room noise for one second...",juce::dontSendNotification);};
    panic.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff863f45));panic.onClick=[this]{p.requestPanic();status.setText("All notes stopped.",juce::dontSendNotification);};
    test.onClick=[this]{p.requestTestNote();status.setText("Sending C4 / MIDI 60 on channel 1 for 350 ms.",juce::dontSendNotification);};
    test.setTooltip("Tests MIDI routing independently of your microphone. Turn on Preview sound for a local test tone.");
    monitor.setTooltip("Pass microphone audio to the output. Use headphones to prevent feedback.");
    preview.setTooltip("Simple local sine instrument for setup. Turn off when using your DAW synth.");
    helpText.setMultiLine(true);helpText.setReadOnly(true);helpText.setScrollbarsShown(true);helpText.setCaretVisible(false);
    helpText.setColour(juce::TextEditor::backgroundColourId,background);helpText.setColour(juce::TextEditor::outlineColourId,gold);helpText.setFont(juce::FontOptions(16));helpText.setText(manualText());
    addAndMakeVisible(helpText);addAndMakeVisible(closeHelp);help.onClick=[this]{instrumentVisible=false;connectionVisible=true;connectionPanel->begin(p.eventCount());connectionPanel->update(audioRunning(),p.peak(),p.level(),p.eventCount(),setupStatus.getText(),p.apvts.getRawParameterValue("synthEnabled")->load()>0.5f||p.apvts.getRawParameterValue("previewAudio")->load()>0.5f);resized();};closeHelp.onClick=[this]{showHelp(false);};
    record.onClick=[this]{if(p.capture.isRecording()){p.capture.stop();record.setButtonText("Stopping...");}else beginCapture();};
    exportMidi.onClick=[this]{exportCapture();};exportMidi.setEnabled(false);
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);bpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,24);bpmSlider.setNumDecimalPlacesToDisplay(0);addAndMakeVisible(bpmSlider);attachSlider("captureBpm",bpmSlider);bpmSlider.textFromValueFunction=[](double v){return juce::String(v,0);};bpmSlider.updateText();
    bpmLabel.setText("BPM",juce::dontSendNotification);addAndMakeVisible(bpmLabel);
    for(int i=0;i<8;++i){trainTarget.addItem("Pad "+juce::String(i+1),i+1);addAndMakeVisible(drumPads[i]);addAndMakeVisible(drumNotes[i]);
        drumPads[i].onClick=[this,i]{trainTarget.setSelectedId(i+1);status.setText("Pad "+juce::String(i+1)+" selected. Press Train 5 hits.",juce::dontSendNotification);};
        drumNotes[i].setSliderStyle(juce::Slider::IncDecButtons);drumNotes[i].setTextBoxStyle(juce::Slider::TextBoxLeft,false,45,22);drumNotes[i].setNumDecimalPlacesToDisplay(0);drumNotes[i].setTooltip("MIDI drum note number, output on channel 10.");attachSlider(drumIDs[i],drumNotes[i]);}
    trainTarget.setSelectedId(1);train.onClick=[this]{p.beats.train(trainTarget.getSelectedId()-1);p.requestPanic();};cancelTrain.onClick=[this]{p.beats.cancel();};clearTrain.onClick=[this]{p.beats.clearModels();status.setText("Drum profiles reset; basic kick/snare/hat detection restored.",juce::dontSendNotification);};
    beatSlider.setSliderStyle(juce::Slider::LinearHorizontal);beatSlider.setTextBoxStyle(juce::Slider::TextBoxRight,false,60,22);beatSlider.setNumDecimalPlacesToDisplay(3);addAndMakeVisible(beatSlider);attachSlider("beatThreshold",beatSlider);beatSlider.textFromValueFunction=[](double v){return juce::String(v,3);};beatSlider.updateText();
    beatLabel.setText("Hit threshold",juce::dontSendNotification);addAndMakeVisible(beatLabel);
    addAndMakeVisible(saveRecent);addAndMakeVisible(rememberMidi);
    buttons.push_back(std::make_unique<ButtonAttachment>(p.apvts,"retroEnabled",rememberMidi));
    saveRecent.onClick=[this]{saveRetrospective();};
    rememberMidi.setTooltip("Keeps up to 30 seconds of generated voice MIDI in memory while audio runs. No microphone audio or automatic disk recording.");
    saveRecent.setTooltip("Freeze the recent MIDI now, then choose where to save. Your current recorded take is preserved.");
    addAndMakeVisible(instrumentButton);
    instrumentPanel=std::make_unique<InstrumentPanel>(p.apvts,[this]{instrumentVisible=false;resized();},[this]{p.requestPanic();});
    addChildComponent(*instrumentPanel);
    instrumentButton.onClick=[this]{connectionVisible=false;instrumentVisible=!instrumentVisible;resized();};
    addAndMakeVisible(audioLabButton);audioLabButton.onClick=[this]{connectionVisible=false;instrumentVisible=false;showHelp(false);resized();openAudioLab();};
    connectionPanel=std::make_unique<ConnectionPanel>([this]{connectionVisible=false;resized();},[this]{showHelp(true);},[this]{p.requestTestNote();},[this](bool local){p.requestPanic();setValue("synthEnabled",local?1.0f:0.0f);setValue("previewAudio",0);setValue("monitorMic",0);},[this]{juce::SystemClipboard::copyTextToClipboard(diagnosticReport());});
    addChildComponent(*connectionPanel);
    setupV5();history.fill(-1);refreshPresets();syncScale();showHelp(false);resized();timerCallback();startTimerHz(30);
}
IDWVoiceMIDIStudioAudioProcessorEditor::~IDWVoiceMIDIStudioAudioProcessorEditor(){stopTimer();p.takes.drain();p.takes.checkpoint();setLookAndFeel(nullptr);}
void IDWVoiceMIDIStudioAudioProcessorEditor::configureSlider(juce::Slider& s,juce::Label& l,const juce::String& text,const juce::String& tip){s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,85,22);s.setTooltip(tip);l.setText(text,juce::dontSendNotification);l.setJustificationType(juce::Justification::centred);l.setColour(juce::Label::textColourId,muted);addAndMakeVisible(s);addAndMakeVisible(l);}
void IDWVoiceMIDIStudioAudioProcessorEditor::refreshPresets(){
    presetSelector.clear(juce::dontSendNotification);const auto factories=p.presets.factoryPresetNames();factoryCount=factories.size();
    for(int i=0;i<factoryCount;++i)presetSelector.addItem(factories[i],i+1);
    userNames=p.presets.list();if(!userNames.isEmpty())presetSelector.addSeparator();for(int i=0;i<userNames.size();++i)presetSelector.addItem(userNames[i],1000+i);
    presetSelector.setTextWhenNothingSelected("Choose a preset");
}
void IDWVoiceMIDIStudioAudioProcessorEditor::loadPreset(){
    const int id=presetSelector.getSelectedId();bool ok=false;
    if(id>0&&id<=factoryCount){ok=p.presets.applyFactoryPreset(p.presets.factoryPresetNames()[id-1]);p.requestPanic();}
    else if(juce::isPositiveAndBelow(id-1000,userNames.size())){ok=p.presets.load(userNames[id-1000]);if(ok)p.restoreExtraState();}
    if(ok){syncScale();status.setText("Preset loaded. Calibrate again if your room or microphone changed.",juce::dontSendNotification);}
}
void IDWVoiceMIDIStudioAudioProcessorEditor::syncScale(){const int mask=(int)p.apvts.getRawParameterValue("scaleMask")->load(),root=(int)p.apvts.getRawParameterValue("root")->load();for(int i=0;i<12;++i){scaleButtons[i].setButtonText(names[(i+root)%12]);scaleButtons[i].setToggleState((mask&(1<<i))!=0,juce::dontSendNotification);}}
void IDWVoiceMIDIStudioAudioProcessorEditor::showHelp(bool visible){helpVisible=visible;helpText.setVisible(visible);closeHelp.setVisible(visible);if(visible){helpText.toFront(false);closeHelp.toFront(false);}}
void IDWVoiceMIDIStudioAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(background);const float w=(float)getWidth();
    if(performanceView){
        g.setColour(card);for(auto r:{juce::Rectangle<float>(24,78,w-48,150),{24,250,w-48,165},{24,435,w-48,190},{24,646,w-48,145}})g.fillRoundedRectangle(r,12);
        g.setColour(gold);g.setFont(juce::FontOptions(12,juce::Font::bold));g.drawText("V6.4",getWidth()-355,22,330,30,juce::Justification::centredRight);
        g.setColour(muted);g.setFont(juce::FontOptions(12));g.drawText("IN DA WIND ENTERTAINMENT / PERFORMANCE",30,getHeight()-24,650,22,juce::Justification::centredLeft);
        return;
    }
    g.setColour(card);for(auto r:{juce::Rectangle<float>(24,78,w-48,150),{24,242,w-48,170},{24,592,w-48,198}})g.fillRoundedRectangle(r,12);
    g.setColour(gold);g.setFont(juce::FontOptions(12,juce::Font::bold));g.drawText("V6.4",getWidth()-335,22,310,30,juce::Justification::centredRight);
    const juce::Rectangle<float> plot(365,110,w-420,93);
    g.setColour(juce::Colour(0xff263144));for(int j=0;j<=4;++j){const float y=plot.getY()+j*plot.getHeight()/4;g.drawHorizontalLine((int)y,plot.getX(),plot.getRight());}
    float centre=60;const float current=history[(size_t)((historyPos+219)%220)];if(current>=0)centre=std::round(current/12)*12;
    juce::Path path;bool pen=false;
    for(int i=0;i<220;++i){const float v=history[(size_t)((historyPos+i)%220)];if(v<0){pen=false;continue;}const float x=plot.getX()+i*plot.getWidth()/219;const float y=juce::jlimit(plot.getY(),plot.getBottom(),plot.getCentreY()-(v-centre)*plot.getHeight()/24);
        if(!pen){path.startNewSubPath(x,y);pen=true;}else path.lineTo(x,y);}
    g.setColour(mint);g.strokePath(path,juce::PathStrokeType(2));
    g.setColour(muted);g.setFont(juce::FontOptions(12));g.drawText("VOICE -> MIDI -> YOUR INSTRUMENT",26,getHeight()-24,420,22,juce::Justification::centredLeft);
    g.drawText("65-1000 Hz  |  5 ms analysis steps",getWidth()-400,getHeight()-24,370,22,juce::Justification::centredRight);
}
void IDWVoiceMIDIStudioAudioProcessorEditor::resized(){
    const int w=getWidth(),inner=w-64;
    title.setBounds(25,18,650,40);readout.setBounds(42,98,305,48);traceLabel.setBounds(367,84,360,22);status.setBounds(42,157,310,53);
    const int knobW=(inner-300)/4;
    juce::Slider* ss[]={&gateSlider,&confidenceSlider,&bendSlider,&tuneSlider};juce::Label* ls[]={&gateLabel,&confidenceLabel,&bendLabel,&tuneLabel};
    for(int i=0;i<4;++i){ls[i]->setBounds(35+i*knobW,252,knobW,23);ss[i]->setBounds(35+i*knobW,277,knobW,121);}
    const int right=w-310;calibrate.setBounds(right,261,270,32);inputSelector.setBounds(right,303,270,30);melody.setBounds(right,346,110,26);mpeToggle.setBounds(right+150,346,100,26);
    scaleLock.setBounds(29,426,112,28);rootSelector.setBounds(149,426,75,28);expressionSelector.setBounds(236,426,165,28);
    const int keyW=(w-440)/12;for(int i=0;i<12;++i)scaleButtons[i].setBounds(420+i*keyW,426,keyW-5,29);
    presetSelector.setBounds(32,472,265,30);save.setBounds(307,472,123,30);learnTarget.setBounds(447,472,145,30);learn.setBounds(601,472,98,30);clearLearn.setBounds(708,472,92,30);help.setBounds(w-182,472,150,30);
    record.setBounds(32,527,135,34);exportMidi.setBounds(178,527,135,34);bpmLabel.setBounds(326,531,40,25);bpmSlider.setBounds(367,527,180,34);captureLabel.setBounds(565,527,w-597,38);
    drumLabel.setBounds(40,601,570,28);beatbox.setBounds(w-155,601,115,28);
    const int padW=inner/8;for(int i=0;i<8;++i){drumPads[i].setBounds(32+i*padW,644,padW-10,43);drumNotes[i].setBounds(38+i*padW,695,padW-22,25);}
    trainTarget.setBounds(40,745,110,28);train.setBounds(160,745,125,28);cancelTrain.setBounds(295,745,80,28);clearTrain.setBounds(385,745,105,28);beatLabel.setBounds(510,745,112,28);beatSlider.setBounds(624,745,w-670,28);
    preview.setBounds(30,804,145,28);monitor.setBounds(185,804,165,28);test.setBounds(369,802,116,32);panic.setBounds(496,802,105,32);diagnostics.setBounds(617,796,w-642,50);
    setupStatus.setBounds(30,836,w-235,25);copyDiagnostics.setBounds(w-195,836,163,25);
    helpText.setBounds(25,76,w-50,getHeight()-128);closeHelp.setBounds(w-152,86,108,30);
    layoutV5();
}
void IDWVoiceMIDIStudioAudioProcessorEditor::beginCapture(){
    if(!audioRunning()){status.setText("Start audio processing before recording.",juce::dontSendNotification);return;}
    if(!p.takes.start(p.apvts.getRawParameterValue("captureBpm")->load())){status.setText("Could not preserve the previous take. Export it before recording again.",juce::dontSendNotification);return;}
    record.setButtonText("Stop recording");exportMidi.setEnabled(false);
}
void IDWVoiceMIDIStudioAudioProcessorEditor::drainCapture(){
    p.takes.drain();const bool recording=p.capture.isRecording();
    record.setButtonText(recording?"Stop recording":"Record MIDI");
    exportMidi.setEnabled(!recording&&p.takes.size()>0);recover.setEnabled(!recording);
    bpmSlider.setEnabled(!recording);
    captureLabel.setText((recording?"RECORDING  ":"TAKE  ")+juce::String(p.takes.duration(),1)+" s / "+juce::String((int)p.takes.size())+" events"+(p.takes.isTruncated()?" [capacity reached]":"")+(p.takes.diskError()?" [BACKUP FAILED: export now]":""),juce::dontSendNotification);
}
bool IDWVoiceMIDIStudioAudioProcessorEditor::writeMidi(const juce::File& file){return p.takes.exportTo(file);}
void IDWVoiceMIDIStudioAudioProcessorEditor::exportCapture(){
    if(p.takes.size()==0||p.capture.isRecording())return;
    chooser=std::make_unique<juce::FileChooser>("Export your MIDI take",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IDW-Take.mid"),"*.mid");
    const juce::Component::SafePointer<IDWVoiceMIDIStudioAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe](const juce::FileChooser& c){if(!safe)return;const auto file=c.getResult();if(file==juce::File{})return;const bool ok=safe->writeMidi(file.withFileExtension("mid"));safe->status.setText(ok?"MIDI exported. Drag it into your DAW.":"Could not write MIDI file.",juce::dontSendNotification);});
}
void IDWVoiceMIDIStudioAudioProcessorEditor::timerCallback(){
    const auto now=juce::Time::getMillisecondCounterHiRes();
    const auto callbacks=p.audioCallbacks();if(callbacks!=lastCallbacks){lastCallbacks=callbacks;lastAudioChange=now;}
    const auto param=[this](const char* id){return p.apvts.getRawParameterValue(id)->load();};
    setupStatus.setText(idw::guidance(idw::diagnose(audioRunning(),p.peak(),p.level(),param("gate"),p.beats.trainingPad()>=0,param("melody")>0.5f,param("beatbox")>0.5f,p.conf(),param("confidence"),p.note()>=0,p.hz()<=0 || (69+12*std::log2(p.hz()/440)>=juce::jmin(param("voiceLow"),param("voiceHigh"))-0.5f && 69+12*std::log2(p.hz()/440)<=juce::jmax(param("voiceLow"),param("voiceHigh"))+0.5f))),juce::dontSendNotification);
    if(calibrating){calibrationPeak=juce::jmax(calibrationPeak,p.peak());noisePeak=juce::jmax(noisePeak,p.level());if(juce::Time::getMillisecondCounterHiRes()-calibrationStarted>=1000){calibrating=false;calibrate.setEnabled(true);if(!audioRunning()||calibrationPeak<0.00001f||calibrationPeak>=0.98f){status.setText("Calibration skipped: input is silent, stopped or clipping. Check mic settings.",juce::dontSendNotification);}else{auto* param=p.apvts.getParameter("gate");param->beginChangeGesture();param->setValueNotifyingHost(param->convertTo0to1(juce::jlimit(0.001f,0.2f,juce::jmax(0.002f,noisePeak*2.5f))));param->endChangeGesture();status.setText("Noise gate calibrated. Sing a steady note.",juce::dontSendNotification);}}}
    const float hz=p.hz();const int n=p.note();history[(size_t)historyPos]=hz>0?69+12*std::log2(hz/440):-1;historyPos=(historyPos+1)%220;
    readout.setText(n>=0?juce::MidiMessage::getMidiNoteName(n,true,true,4)+"  /  "+juce::String(hz,1)+" Hz":"Ready for your voice",juce::dontSendNotification);
    const auto db=juce::Decibels::gainToDecibels(p.level(),-100.0f);
    diagnostics.setText("IN "+juce::String(db,1)+" dB  |  CONF "+juce::String(p.conf(),2)+"  |  LOAD "+juce::String(p.callbackLoad()*100,1)+"%\nMIDI: "+juce::String(p.noteCount())+" notes / "+juce::String(p.eventCount())+" events",juce::dontSendNotification);
    const auto mode=(int)param("harmonyMode");
    arrangementStatus.setText(param("mpe")>0.5f&&mode>0?"MPE active: chord and bass layers are suppressed.":mode==0?"Lead only. Choose Major or Minor to add an arrangement.":"Lead CH 1 / Chords CH 2 / Bass CH 3. Preview plays the lead only.",juce::dontSendNotification);
    if(learningRange){
        if(p.hz()>0&&p.conf()>=param("confidence")&&p.level()>=param("gate")){
            const int note=juce::jlimit(0,127,(int)std::lround(69+12*std::log2(p.hz()/440)));
            learnedLow=juce::jmin(learnedLow,note);learnedHigh=juce::jmax(learnedHigh,note);++rangeSamples;
        }
        if(now-rangeStarted>=12000){learningRange=false;learnRange.setButtonText("Learn range");
            if(rangeSamples>=15){setValue("voiceLow",(float)juce::jmax(0,learnedLow-2));setValue("voiceHigh",(float)juce::jmin(127,learnedHigh+2));status.setText("Range learned with two-note margins. Name and save your voice profile.",juce::dontSendNotification);}
            else status.setText("Not enough clear singing. Existing range kept; check the microphone.",juce::dontSendNotification);
        }
    }
    const int training=p.beats.trainingPad();
    if(training>=0)drumLabel.setText("TRAIN PAD "+juce::String(training+1)+"  /  "+juce::String(p.beats.examplesRemaining())+" hits remaining - leave a gap between hits",juce::dontSendNotification);
    else drumLabel.setText("DRUM LAB  /  Train each pad with five distinct hits",juce::dontSendNotification);
    if(previousDrumEvents!=p.drumEventCount()){previousDrumEvents=p.drumEventCount();flashPad=p.lastDrum();flashTicks=6;}
    if(flashTicks>0)--flashTicks;
    for(int i=0;i<8;++i){drumPads[i].setButtonText(juce::String(i+1)+(p.beats.examples(i)>=3?" / trained":i==0?" / kick":i==1?" / snare":i==2?" / hat":" / empty"));drumPads[i].setToggleState(training==i||(flashTicks>0&&flashPad==i),juce::dontSendNotification);}
    if(connectionPanel&&connectionVisible)connectionPanel->update(audioRunning(),p.peak(),p.level(),p.eventCount(),setupStatus.getText(),param("synthEnabled")>0.5f||param("previewAudio")>0.5f);
    train.setEnabled(training<0);cancelTrain.setEnabled(training>=0);syncScale();drainCapture();repaint(24,78,getWidth()-48,150);
}
bool IDWVoiceMIDIStudioAudioProcessorEditor::audioRunning() const {
    return lastAudioChange>0 && juce::Time::getMillisecondCounterHiRes()-lastAudioChange<1000;
}
juce::String IDWVoiceMIDIStudioAudioProcessorEditor::diagnosticReport() const {
    juce::String text="IDW Voice MIDI Studio 6.4.0 setup report\n";
    text += "Audio callbacks active: "+juce::String(audioRunning()?"yes":"no")+"\n";
    text += "Sample rate: "+juce::String(p.deviceRate(),0)+" Hz; block: "+juce::String(p.deviceBlock())+" samples\n";
    text += "Input RMS: "+juce::String(juce::Decibels::gainToDecibels(p.level(),-100.0f),1)+" dBFS; peak: "+juce::String(p.peak(),4)+"\n";
    for(const char* id:{"inputMode","gate","confidence","melody","beatbox","previewAudio","monitorMic","mpe","bend","harmonyMode","harmonyVoicing","harmonyBass","voiceLow","voiceHigh","synthEnabled","synthGain","retroEnabled"})
        text += juce::String(id)+": "+juce::String(p.apvts.getRawParameterValue(id)->load(),3)+"\n";
    text += "Pitch: "+juce::String(p.hz(),1)+" Hz; confidence: "+juce::String(p.conf(),2)+"\n";
    text += "Generated MIDI: "+juce::String(p.noteCount())+" notes, "+juce::String(p.eventCount())+" events\n";
    text += "Status: "+setupStatus.getText()+"\nDAW receipt, instrument selection and audio output audibility are not detectable by this plugin.\n";
    if(connectionPanel)text+=connectionPanel->report();
    return text;
}
juce::String IDWVoiceMIDIStudioAudioProcessorEditor::manualText(){return R"HELP(IDW VOICE MIDI STUDIO / VERSION 6.4

CONNECTION CHECK
Setup / Help opens live signal readings and route instructions. Send a test note, then manually confirm if audible. Full manual returns here. Audio Lab opens the bundled companion or asks you to locate its EXE.

PERFORMANCE VIEW
Open Studio instrument to enable the built-in 32-voice synth and choose lead, chord and bass sounds. It replaces Preview sound while enabled. Audio Lab is a separate companion for file transcription and optional cloud conversion.

Switch between the performance screen and Studio controls at the top.
Choose Major or Minor and a root to harmonize your voice into diatonic triads or sevenths.
Lead: MIDI channel 1. Chords: channel 2. Optional bass: channel 3. Drums: channel 10.
Route these channels to separate instruments or a multitimbral instrument in your DAW.
Chord/bass notes stay centered; voice bends apply to the lead. Preview plays the lead only.
Harmony follows the nearest scale degree, with ties choosing the lower note.
MPE suppresses chord/bass layers to avoid member-channel conflicts.

VOICE PROFILES
Enter a name in the voice-profile box. Save voice stores only input mode, gate,
confidence, tuning and low/high voice limits. It does not change musical presets.
Learn range listens for 12 seconds. Sing your comfortable low and high notes.
Click Cancel range to keep the existing range. Save the profile after learning.
Voice limits use MIDI note numbers; 60 is middle C. Swap reversed endpoints automatically.
Profiles do not choose the physical microphone device. Keep separate named profiles per mic.

RECOVERABLE TAKES
Recording continues when you close the editor window. The take belongs to the processor.
Recovery MIDI files are checkpointed approximately every five seconds while the host
message thread is responsive, and at stop/close. Backups are local; no audio is uploaded.
Recover take opens the Recovered Takes folder. Choose a .mid backup to restore and export.
This does not guarantee zero data loss in a crash: the latest checkpoint is the recovery point.
If backup fails, export manually. Starting a new take first preserves the old one.


LIVE SETUP CHECK
The bottom status line checks audio activity, input level, gate and pitch confidence.
Copy diagnostics copies settings and signal readings for support; it contains no recording.
If calibration sees silence, clipping or stopped audio, it keeps your previous gate.
MIDI counts mean generated events, not confirmed delivery to a DAW instrument.

FIRST SOUND
1. In the standalone Audio/MIDI settings, choose your microphone and speakers/headphones.
2. Enable Preview sound. Test note plays a short C4 tone and also sends MIDI.
3. Stay quiet and Calibrate noise. Sing a steady note. The green readout shows accepted MIDI notes.
4. Preview sound is a simple setup instrument. Turn it off when listening through a DAW synth.

FL STUDIO / VST3
1. Load IDW as an effect on the Mixer insert receiving your microphone.
2. Open IDW's wrapper Settings and set MIDI Output port to 10 (or another unused port).
3. Load your destination instrument and set its wrapper MIDI Input port to the same number.
4. Press Test note. It sends MIDI 60 on channel 1, independently of vocal detection.
5. If the MIDI counter increases but your synth is silent, check routing and the synth's channel.
6. Match Bend Range in your synth. IDW sends RPN pitch-bend sensitivity; some instruments ignore it.
7. Start with Clean Vocal. Calibrate after loading a preset, since presets change the noise gate.

STANDALONE MIDI
Select a MIDI output in the standalone Audio/MIDI settings. For FL Studio on the same computer,
use an existing virtual MIDI port (such as loopMIDI), enable that port as an input in FL Studio,
and select a receiving instrument. The standalone app does not install a virtual MIDI driver.

SCALE MODES
Scale lock OFF: free pitch, with MIDI note changes plus pitch bends.
Strict scale: both note selection and sounding pitch stay on the selected scale.
Natural vibrato: scale note plus up to 45 cents of within-note vocal expression.
The twelve buttons are relative to the selected root and display the resulting note names.
At least one scale note must remain enabled.

DRUM LAB
Enable Beatbox for drum output on MIDI channel 10. Change each pad's MIDI note below its button.
With no trained pads, the engine uses basic kick/snare/hat spectral rules.
Select a pad, press Train 5 hits, and make five distinct examples with pauses between them.
Training suppresses melody and drum output while it learns. Repeat for additional pads.
Once profiles exist, only trained pads are classified. Ambiguous/unfamiliar hits are rejected.
These are local spectral templates, not a neural model. Accuracy depends on microphone,
background noise and distinct sounds. Save a preset to retain your profiles.
Reset pads removes all learned profiles and restores basic three-sound detection.

CAPTURE / EXPORT
Set BPM to your DAW tempo before recording. Record MIDI, perform, then Stop recording.
Export MIDI saves notes, drums, bends and controller expression. Drag the .mid file into your DAW.
A take is limited to ten minutes / 250,000 displayed events. An overflow is visibly marked.
The current take survives closing the editor. Export a copy for your project and keep recovery files.
Capture starts from new MIDI events; begin before singing. Notes held when stopping are closed
in the exported file, without stopping your live instrument. Tempo is fixed for each take.

SAFETY / TROUBLESHOOTING
PANIC stops sounding MIDI notes. Notes resume after a short suppression interval.
Hear microphone passes raw microphone audio to the output; use headphones to prevent feedback.
MPE reserves channel 10 for drums. Choose an MPE synth and match its bend range.
MIDI Learn is channel-aware. Mappings and drum profiles save in user presets and DAW sessions.
Changing input devices stops an active capture. Export the take before switching devices.
The displayed LOAD is callback time as a percentage of the audio block budget, not total CPU.
Lower host buffers reduce device latency; pitch estimation still needs a short window of sound.

LIMITS
Monophonic voice tracking, 65-1000 Hz. This version does not provide polyphonic transcription,
AI voice cloning, cloud processing, a full synthesizer, or automatic access to DAW routing.
)HELP";}

void IDWVoiceMIDIStudioAudioProcessorEditor::setValue(const char* id,float value){
    if(auto* parameter=p.apvts.getParameter(id)){parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(value));parameter->endChangeGesture();}
}
void IDWVoiceMIDIStudioAudioProcessorEditor::refreshProfiles(){
    const auto selected=profileSelector.getText();profileSelector.clear(juce::dontSendNotification);
    profileNames=VoiceProfiles::names();for(int i=0;i<profileNames.size();++i)profileSelector.addItem(profileNames[i],i+1);
    profileSelector.setText(selected,juce::dontSendNotification);
}
void IDWVoiceMIDIStudioAudioProcessorEditor::setupV5(){
    for(auto* c:std::initializer_list<juce::Component*>{&viewButton,&saveProfile,&learnRange,&recover,&harmonyMode,&harmonyVoicing,&profileSelector,&bassLayer,&voiceLow,&voiceHigh,&arrangementTitle,&arrangementStatus,&profileTitle,&lowLabel,&highLabel})addAndMakeVisible(c);
    viewButton.onClick=[this]{performanceView=!performanceView;resized();repaint();};
    arrangementTitle.setText("ONE VOICE / FULL ARRANGEMENT",juce::dontSendNotification);arrangementTitle.setColour(juce::Label::textColourId,gold);
    profileTitle.setText("YOUR VOICE / YOUR MICROPHONE",juce::dontSendNotification);profileTitle.setColour(juce::Label::textColourId,gold);
    harmonyMode.addItem("Harmony off",1);harmonyMode.addItem("Major scale",2);harmonyMode.addItem("Natural minor",3);
    harmonyVoicing.addItem("Triad",1);harmonyVoicing.addItem("Seventh",2);
    combos.push_back(std::make_unique<ComboAttachment>(p.apvts,"harmonyMode",harmonyMode));
    combos.push_back(std::make_unique<ComboAttachment>(p.apvts,"harmonyVoicing",harmonyVoicing));
    buttons.push_back(std::make_unique<ButtonAttachment>(p.apvts,"harmonyBass",bassLayer));
    for(auto* slider:{&voiceLow,&voiceHigh}){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,55,26);slider->setNumDecimalPlacesToDisplay(0);slider->setTooltip("MIDI note number, 60 = middle C. Notes outside this voice range are ignored.");}
    sliders.push_back(std::make_unique<SliderAttachment>(p.apvts,"voiceLow",voiceLow));sliders.push_back(std::make_unique<SliderAttachment>(p.apvts,"voiceHigh",voiceHigh));
    lowLabel.setText("Lowest note",juce::dontSendNotification);highLabel.setText("Highest note",juce::dontSendNotification);
    profileSelector.setEditableText(true);profileSelector.setTextWhenNothingSelected("Type a voice profile name");refreshProfiles();
    profileSelector.onChange=[this]{const int index=profileSelector.getSelectedId()-1;if(juce::isPositiveAndBelow(index,profileNames.size())){const bool ok=VoiceProfiles::load(p.apvts,profileNames[index]);if(ok)p.requestPanic();status.setText(ok?"Voice profile loaded. Check the physical microphone selection.":"Could not load this voice profile.",juce::dontSendNotification);}};
    saveProfile.onClick=[this]{const auto name=VoiceProfiles::safeName(profileSelector.getText());const bool ok=VoiceProfiles::save(p.apvts,name);if(ok){profileSelector.setText(name,juce::dontSendNotification);refreshProfiles();}status.setText(ok?"Voice profile saved.":"Enter a profile name; check that your user folder is writable.",juce::dontSendNotification);};
    learnRange.onClick=[this]{learningRange=!learningRange;learnRange.setButtonText(learningRange?"Cancel range":"Learn range");if(learningRange){rangeStarted=juce::Time::getMillisecondCounterHiRes();learnedLow=127;learnedHigh=0;rangeSamples=0;status.setText("For 12 seconds, sing your comfortable low and high notes.",juce::dontSendNotification);}};
    recover.onClick=[this]{recoverTake();};
}
void IDWVoiceMIDIStudioAudioProcessorEditor::recoverTake(){
    if(p.capture.isRecording())return;
    chooser=std::make_unique<juce::FileChooser>("Recover an IDW MIDI take",TakeArchive::directory(),"*.mid");
    const juce::Component::SafePointer<IDWVoiceMIDIStudioAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const juce::FileChooser& c){if(!safe||c.getResult()==juce::File{})return;const bool ok=safe->p.takes.recover(c.getResult());safe->status.setText(ok?"Take recovered. Export MIDI to keep a project copy.":"Could not recover this file; current take kept.",juce::dontSendNotification);});
}
void IDWVoiceMIDIStudioAudioProcessorEditor::layoutV5(){
    const int w=getWidth();
    for(auto* child:getChildren())child->setVisible(!performanceView);
    for(auto* c:std::initializer_list<juce::Component*>{&harmonyMode,&harmonyVoicing,&bassLayer,&arrangementTitle,&arrangementStatus,&profileTitle,&profileSelector,&saveProfile,&learnRange,&voiceLow,&voiceHigh,&lowLabel,&highLabel,&recover})c->setVisible(performanceView);
    title.setBounds(25,18,500,40);viewButton.setBounds(545,24,140,30);viewButton.setVisible(true);viewButton.setButtonText(performanceView?"Studio controls":"Performance view");
    if(performanceView){
        for(auto* c:std::initializer_list<juce::Component*>{&title,&readout,&status,&diagnostics,&setupStatus,&copyDiagnostics,&preview,&test,&panic,&record,&exportMidi,&captureLabel,&bpmLabel,&bpmSlider,&help,&rootSelector,&calibrate})c->setVisible(true);
        readout.setBounds(42,100,w-84,58);readout.setFont(juce::FontOptions(36.0f,juce::Font::bold));status.setBounds(42,164,w-84,45);
        arrangementTitle.setBounds(40,260,w-80,30);harmonyMode.setBounds(42,306,210,36);rootSelector.setBounds(267,306,90,36);harmonyVoicing.setBounds(372,306,170,36);bassLayer.setBounds(563,306,140,36);arrangementStatus.setBounds(42,355,w-84,46);
        profileTitle.setBounds(40,446,w-80,28);profileSelector.setBounds(42,485,350,34);saveProfile.setBounds(405,485,130,34);learnRange.setBounds(548,485,140,34);calibrate.setBounds(w-295,485,250,34);
        lowLabel.setBounds(42,544,110,28);voiceLow.setBounds(152,540,270,36);highLabel.setBounds(452,544,110,28);voiceHigh.setBounds(562,540,270,36);
        record.setBounds(42,667,190,45);exportMidi.setBounds(245,667,150,45);recover.setBounds(408,667,155,45);bpmLabel.setBounds(587,674,40,28);bpmSlider.setBounds(629,667,200,45);
        captureLabel.setBounds(42,727,w-84,45);preview.setBounds(32,801,145,34);test.setBounds(190,801,140,34);panic.setBounds(344,801,135,34);help.setBounds(w-205,801,165,34);diagnostics.setBounds(498,792,w-720,45);
    }else{readout.setFont(juce::FontOptions(31.0f,juce::Font::bold));}
    rememberMidi.setVisible(performanceView);saveRecent.setVisible(performanceView);
    if(performanceView){saveRecent.setBounds(w-195,670,150,38);rememberMidi.setBounds(w-250,737,210,28);captureLabel.setBounds(42,727,w-330,45);}
    audioLabButton.setVisible(true);audioLabButton.setBounds(865,24,105,30);
    instrumentButton.setVisible(true);instrumentButton.setBounds(700,24,150,30);
    if(instrumentPanel){instrumentPanel->setBounds(25,78,w-50,getHeight()-128);instrumentPanel->setVisible(instrumentVisible);}
    if(instrumentVisible&&instrumentPanel)instrumentPanel->toFront(false);
    if(connectionPanel){connectionPanel->setBounds(25,78,w-50,getHeight()-128);connectionPanel->setVisible(connectionVisible);if(connectionVisible)connectionPanel->toFront(false);}
    helpText.setVisible(helpVisible);closeHelp.setVisible(helpVisible);if(helpVisible){helpText.toFront(false);closeHelp.toFront(false);}
}

void IDWVoiceMIDIStudioAudioProcessorEditor::saveRetrospective(){
    if(p.apvts.getRawParameterValue("retroEnabled")->load()<0.5f){status.setText("Enable Remember voice MIDI, then perform with audio running.",juce::dontSendNotification);return;}
    auto take=std::make_shared<std::vector<PerformanceCapture::Event>>(p.retrospective.snapshot());
    if(take->empty()){status.setText("No recent voice notes yet. Sing with Melody enabled and audio running.",juce::dontSendNotification);return;}
    const double tempo=p.apvts.getRawParameterValue("captureBpm")->load();const bool incomplete=p.retrospective.incomplete();
    chooser=std::make_unique<juce::FileChooser>("Save retrospective MIDI",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IDW-recent.mid"),"*.mid");
    const juce::Component::SafePointer<IDWVoiceMIDIStudioAudioProcessorEditor> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe,take,tempo,incomplete](const juce::FileChooser& c){
        if(!safe||c.getResult()==juce::File{})return;juce::TemporaryFile temp(c.getResult());
        const bool ok=writePerformanceMidi(temp.getFile(),*take,tempo)&&temp.overwriteTargetFileWithTemporary();
        safe->status.setText(ok?(incomplete?"Recent MIDI saved; a buffer gap/limit was detected. Check this take.":"Recent MIDI saved. Your recorded take is unchanged."):"Could not save recent MIDI; check the destination.",juce::dontSendNotification);
    });
}

void IDWVoiceMIDIStudioAudioProcessorEditor::openAudioLab(){
#if JUCE_WINDOWS
    const auto settings=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IDW Voice MIDI Studio").getChildFile("audio-lab-location.txt");
    const auto bundled=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getParentDirectory().getParentDirectory().getChildFile("Audio-Lab/IDW-Audio-Lab.exe");
    const auto valid=[](const juce::File& file){return file.existsAsFile()&&file.getFileName().equalsIgnoreCase("IDW-Audio-Lab.exe");};
    juce::File target=bundled;
    if(!valid(target)){const auto saved=settings.loadFileAsString().trim();if(juce::File::isAbsolutePath(saved))target=juce::File(saved);}
    if(valid(target)){status.setText(target.startAsProcess()?"Audio Lab launch requested.":"Audio Lab could not start. Open Run-Audio-Lab.cmd from the extracted package.",juce::dontSendNotification);return;}
    labChooser=std::make_unique<juce::FileChooser>("Locate Audio-Lab/IDW-Audio-Lab.exe in your extracted IDW download",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"IDW-Audio-Lab.exe");
    audioLabButton.setEnabled(false);
    const juce::Component::SafePointer<IDWVoiceMIDIStudioAudioProcessorEditor> safe(this);
    labChooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,settings,valid](const juce::FileChooser& dialog){
        if(!safe)return;safe->audioLabButton.setEnabled(true);const auto file=dialog.getResult();if(file==juce::File{})return;
        if(!valid(file)){safe->status.setText("Choose IDW-Audio-Lab.exe from the extracted Audio-Lab folder.",juce::dontSendNotification);return;}
        if(!file.startAsProcess()){safe->status.setText("Audio Lab could not start. Keep its entire folder intact.",juce::dontSendNotification);return;}
        settings.getParentDirectory().createDirectory();settings.replaceWithText(file.getFullPathName());
        safe->status.setText("Audio Lab launch requested. Its location is remembered on this PC.",juce::dontSendNotification);
    });
#else
    status.setText("The bundled Audio Lab executable requires Windows. Use the companion Python source on other platforms.",juce::dontSendNotification);
#endif
}
