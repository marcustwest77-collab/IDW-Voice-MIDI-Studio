#include "MidiSineSynth.h"
#include "PlaybackTiming.h"
#include "LoopbackWavAnalysis.h"
#include "LoopbackReference.h"
#include "LoopbackCapture.h" // experimental live capture engine; not wired to UI yet
#include "PianoRoll.h"
#include <JuceHeader.h>
#include "SessionModel.h"
#include "TimelineMath.h"
#include "TakeRecorder.h"
#include "MultiTakeRecorder.h" // compile-check experimental multitrack engine
#include "SessionPersistence.h"
#include "ProjectPackaging.h"
#include "OfflineBounce.h"
#include "IDWConnectPanel.h"
#include "AIApprovalPanel.h"
#include "AIStatusSnapshot.h"
#include <atomic>
#include <memory>
#include <thread>
// Neo Studio: visual-only styling; recording and audio callbacks remain unchanged.
class IDWNeoLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    IDWNeoLookAndFeel() {
        setColour(juce::TextButton::buttonColourId,juce::Colour(0xff152638));
        setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff007fa9));
        setColour(juce::TextButton::textColourOffId,juce::Colour(0xffd6ebf8));
        setColour(juce::TextButton::textColourOnId,juce::Colours::white);
        setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff111e2e));
        setColour(juce::ComboBox::outlineColourId,juce::Colour(0xff28506a));
        setColour(juce::ComboBox::textColourId,juce::Colour(0xffd6ebf8));
        setColour(juce::Slider::thumbColourId,juce::Colour(0xff15d4ff));
        setColour(juce::Slider::trackColourId,juce::Colour(0xff127a9e));
        setColour(juce::Slider::backgroundColourId,juce::Colour(0xff182739));
        setColour(juce::Label::textColourId,juce::Colour(0xffd6ebf8));
        setColour(juce::ToggleButton::textColourId,juce::Colour(0xffd6ebf8));
        setColour(juce::ToggleButton::tickColourId,juce::Colour(0xff10d8ff));
        setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff101b2b));
        setColour(juce::TextEditor::textColourId,juce::Colours::white);
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff101b2b));
        setColour(juce::PopupMenu::highlightedBackgroundColourId,juce::Colour(0xff176b94));
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& button,
                              const juce::Colour& background,
                              bool highlighted,bool down) override {
        const auto area=button.getLocalBounds().toFloat().reduced(1.5f);
        const bool recording=button.getButtonText().containsIgnoreCase("RECORD") ||
            button.getButtonText().containsIgnoreCase("STOP RECORD");
        const auto accent=recording ? juce::Colour(0xffe94565) : juce::Colour(0xff12c9ff);
        const auto fill=down ? accent.withAlpha(0.36f) :
            (highlighted ? juce::Colour(0xff214463) : background);
        g.setColour(fill);g.fillRoundedRectangle(area,7.0f);
        g.setColour(accent.withAlpha(highlighted || down ? 0.95f : 0.44f));
        g.drawRoundedRectangle(area,7.0f,highlighted ? 1.6f : 1.0f);
    }
};
class Workstation final : public juce::AudioAppComponent, private juce::Timer {
public:
    Workstation() {
        setSize(1450,850);
        setLookAndFeel(&neoTheme);
        formats.registerBasicFormats();
        setAudioChannels(8,2); // Request up to eight available hardware inputs.
        deviceManager.addAudioCallback(&recorder);
        deviceManager.addAudioCallback(&multiRecorder);
        deviceManager.addAudioCallback(&loopbackCapture);
        title.setText("IDW  |  VOICE MIDI STUDIO  |  NEO STUDIO BETA",juce::dontSendNotification);
        title.setColour(juce::Label::textColourId,juce::Colour(0xff4adfff));
        title.setFont(juce::Font(juce::FontOptions(22.0f,juce::Font::bold)));
        addAndMakeVisible(title);
        for(auto* b:{&play,&stop,&audio,&midi,&open,&record,&saveProject,&openProject,&exportProject,&bounce,&cancelBounce,&connectButton,&aiButton}) addAndMakeVisible(*b);
        play.setButtonText("PLAY"); stop.setButtonText("STOP"); open.setButtonText("OPEN WAV/AIFF");
        saveProject.setButtonText("SAVE PROJECT");
        openProject.setButtonText("OPEN PROJECT");
        exportProject.setButtonText("EXPORT PORTABLE");
        bounce.setButtonText("BOUNCE WAV");
        cancelBounce.setButtonText("CANCEL EXPORT");cancelBounce.setEnabled(false);
        connectButton.setButtonText("IDW CONNECT");
        addChildComponent(connectPanel);
        connectButton.onClick=[this]{
            connectPanel.setVisible(!connectPanel.isVisible());
            connectPanel.toFront(false);
        };
        synthPreset.addItem("Keys",1);
        synthPreset.addItem("Bass",2);
        synthPreset.addItem("Lead",3);
        synthPreset.addItem("Organ",4);
        synthPreset.addItem("Pad",5);
        synthPreset.setSelectedId(1,juce::dontSendNotification);
        addAndMakeVisible(synthPreset);
        synthPreset.onChange=[this]{
            if(playing.load() || selectedTrack>=session.tracks.size())return;
            session.setTrackSynthPreset(selectedTrack,synthPreset.getSelectedId()-1);
        };
        attackLabel.setText("ATTACK ms",juce::dontSendNotification);
        releaseLabel.setText("RELEASE ms",juce::dontSendNotification);
        addAndMakeVisible(attackLabel);addAndMakeVisible(releaseLabel);
        for(auto* slider:{&attackControl,&releaseControl}){
            slider->setRange(0.0,2000.0,1.0);
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,58,20);
            addAndMakeVisible(*slider);
        }
        const auto updateEnvelope=[this]{
            if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load() ||
               selectedTrack>=session.tracks.size())return;
            session.setTrackSynthEnvelope(selectedTrack,
                (float)attackControl.getValue(),(float)releaseControl.getValue());
        };
        attackControl.onValueChange=updateEnvelope;
        releaseControl.onValueChange=updateEnvelope;
        pianoButton.setButtonText("PIANO ROLL");
        addAndMakeVisible(pianoButton);
        addChildComponent(pianoRoll);
        pianoRoll.session=&session;
        pianoButton.onClick=[this]{
            if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
            if(selectedTrack>=session.tracks.size() ||
               session.tracks[selectedTrack].type!=idw::daw::TrackType::midi)return;
            pianoRoll.trackIndex=selectedTrack;
            pianoRoll.setVisible(!pianoRoll.isVisible());
            if(pianoRoll.isVisible()){
                aiPanel.setVisible(false);connectPanel.setVisible(false);
                pianoRoll.toFront(false);pianoRoll.repaint();
            }
        };
        aiButton.setButtonText("IDW AI STUDIO");
        addChildComponent(aiPanel);
        aiButton.onClick=[this]{
            aiPanel.setVisible(!aiPanel.isVisible());
            if(aiPanel.isVisible()){connectPanel.setVisible(false);aiPanel.toFront(false);}
        };
        aiPanel.getSanitizedStatus=[this]{
            return juce::JSON::toString(idw::daw::aiSessionStatus(
                session,playing.load(),(recorder.isRecording() || multiRecorder.isRecording())),true);
        };
        aiPanel.onApproved=[this](const idw::daw::AIProposal& proposal){
            if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return false;
            if(proposal.kind==idw::daw::AIProposal::Kind::addAudioTrack){
                const auto count=session.tracks.size();
                addAudioTrack();
                if(session.tracks.size()==count)return false;
            }else if(!idw::daw::applyApprovedAIProposal(session,proposal))return false;
            if(proposal.kind==idw::daw::AIProposal::Kind::tempo)
                tempo.setValue(session.bpm,juce::sendNotificationSync);
            refreshMixer();repaint();
            return true;
        };
        record.setButtonText("RECORD VOCAL");
        latencyLabel.setText("TAKE TRIM ms",juce::dontSendNotification);
        addAndMakeVisible(latencyLabel);
        latencyAdjust.setRange(-100.0,250.0,1.0);
        latencyAdjust.setValue(0.0);
        latencyAdjust.setTextBoxStyle(juce::Slider::TextBoxRight,false,65,24);
        latencyAdjust.setTooltip("Manual adjustment added to driver-reported input latency (milliseconds). Negative values reduce trim. Loopback measures round-trip delay and must NOT be copied directly into this input-trim control.");
        addAndMakeVisible(latencyAdjust);
        analyzeCalibration.setButtonText("ANALYZE LOOPBACK WAV");
        analyzeCalibration.setTooltip("Measure known test-pulse positions in an existing WAV. Does not change take trim.");
        addAndMakeVisible(analyzeCalibration);
        analyzeCalibration.onClick=[this]{analyzeCalibrationWav();};
        generateCalibration.setButtonText("CREATE PULSE WAV");
        generateCalibration.setTooltip("Generate a safe-level 48 kHz test WAV with pulses at 100, 200, 300, 400, 500 ms.");
        addAndMakeVisible(generateCalibration);
        generateCalibration.onClick=[this]{createCalibrationReference();};
        liveCalibration.setButtonText("LIVE LOOPBACK TEST");
        liveCalibration.setTooltip("Experimental: route audio interface line output 1 to selected line input. Low-volume pulses; headphones disconnected from loopback path.");
        addAndMakeVisible(liveCalibration);
        liveCalibration.onClick=[this]{startLiveCalibration();};
        overdubControl.setButtonText("PLAY BEAT WHILE RECORDING");
        overdubControl.setTooltip("Play existing tracks while capturing new takes on empty armed tracks.");
        overdubControl.setToggleState(false,juce::dontSendNotification);
        addAndMakeVisible(overdubControl);
        audio.setButtonText("+ AUDIO TRACK"); midi.setButtonText("+ MIDI TRACK");
        play.onClick=[this]{if(playing.load() || loopbackCapture.isActive() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;if(loadedCount()>0 || std::any_of(session.tracks.begin(),session.tracks.end(),[](const auto& t){return !t.notes.empty();})){pianoRoll.setVisible(false);
            std::atomic_store_explicit(&playbackSession,
                std::make_shared<const idw::daw::Session>(session),std::memory_order_release);
            sessionFrames.store(0);for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(slot->sourceOffsetSeconds);slot->transport.start();}playing=true;startTimerHz(30);repaint();}};
        stop.onClick=[this]{if(exportRunning.load())return;if(loopbackCapture.isActive() || loopbackCapture.isComplete()){loopbackCapture.cancel();liveCalibration.setEnabled(true);}else if(loopbackCapture.consumeFailed()){loopbackCapture.cancel();liveCalibration.setEnabled(true);}if((recorder.isRecording() || multiRecorder.isRecording()))toggleRecording();for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(slot->sourceOffsetSeconds);}sessionFrames.store(0);playing=false;stopTimer();playheadBeats=0;refreshMixer();repaint();};
        open.onClick=[this]{loadAudio();};
        record.onClick=[this]{toggleRecording();};
        saveProject.onClick=[this]{saveProjectFile();};
        openProject.onClick=[this]{openProjectFile();};
        exportProject.onClick=[this]{exportProjectFile();};
        bounce.onClick=[this]{bounceMix();};
        cancelBounce.onClick=[this]{exportCancel.store(true);};
        audio.onClick=[this]{if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;addAudioTrack();refreshMixer();repaint();};
        midi.onClick=[this]{if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;session.addTrack(idw::daw::TrackType::midi);refreshMixer();repaint();};
        tempo.setRange(20,300,1);tempo.setValue(120);tempo.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,28);
        tempo.onValueChange=[this]{if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;session.setTempo(tempo.getValue());};addAndMakeVisible(tempo);
        tempoLabel.setText("BPM",juce::dontSendNotification);addAndMakeVisible(tempoLabel);
        addAudioTrack();
        session.addTrack(idw::daw::TrackType::midi);
        gainLabel.setText("TRACK 1 VOL",juce::dontSendNotification);
        panLabel.setText("PAN",juce::dontSendNotification);
        for(auto* label:{&gainLabel,&panLabel})addAndMakeVisible(*label);
        gainControl.setRange(0.0,2.0,0.01);gainControl.setValue(1.0);
        panControl.setRange(-1.0,1.0,0.01);panControl.setValue(0.0);
        gainControl.setTextBoxStyle(juce::Slider::TextBoxRight,false,56,26);
        panControl.setTextBoxStyle(juce::Slider::TextBoxRight,false,56,26);
        for(auto* slider:{&gainControl,&panControl})addAndMakeVisible(*slider);
        muteControl.setButtonText("MUTE");addAndMakeVisible(muteControl);
        soloControl.setButtonText("SOLO");addAndMakeVisible(soloControl);
        gainControl.onValueChange=[this]{updateFirstTrackMix();};
        panControl.onValueChange=[this]{updateFirstTrackMix();};
        muteControl.onClick=[this]{
            if(playing.load() || session.tracks.empty())return;
            session.tracks[selectedTrack].mute=muteControl.getToggleState();repaint();
        };
        soloControl.onClick=[this]{
            if(playing.load() || session.tracks.empty())return;
            session.tracks[selectedTrack].solo=soloControl.getToggleState();repaint();
        };
        previousTrack.setButtonText("< TRACK");
        nextTrack.setButtonText("TRACK >");
        addAndMakeVisible(previousTrack);addAndMakeVisible(nextTrack);
        inputChannel.addItemList({"INPUT 1","INPUT 2","INPUT 3","INPUT 4",
            "INPUT 5","INPUT 6","INPUT 7","INPUT 8"},1);
        inputChannel.setSelectedId(1,juce::dontSendNotification);
        inputLevel.setText("INPUT LEVEL: idle",juce::dontSendNotification);
        addAndMakeVisible(inputLevel);
        addAndMakeVisible(inputChannel);
        inputChannel.onChange=[this]{
            if(selectedTrack>=session.tracks.size() || playing.load() ||
               recorder.isRecording() || multiRecorder.isRecording() || exportRunning.load())return;
            const int selectedId=inputChannel.getSelectedId();
            if(selectedId<=0)return; // Device change may clear the selection.
            if(auto* device=deviceManager.getCurrentAudioDevice()){
                const int physicalInput=selectedId-1;
                if(physicalInput>=device->getInputChannelNames().size() ||
                   !device->getActiveInputChannels()[physicalInput])return;
            }else return;
            session.tracks[selectedTrack].inputChannel=selectedId-1;
        };
        monitorControl.setButtonText("HEADPHONES ONLY");
        monitorControl.setTooltip("Dry monitoring of the first armed input; headphones required. OFF by default.");
        monitorControl.setToggleState(false,juce::dontSendNotification);
        monitorControl.setEnabled(false);
        addAndMakeVisible(monitorControl);
        audioDevices.setButtonText("AUDIO DEVICES");
        audioDevices.setTooltip("Select a Windows ASIO driver and enable your PreSonus AudioBox microphone input.");
        addAndMakeVisible(audioDevices);
        audioDevices.onClick=[this]{
            if(playing.load() || recorder.isRecording() || multiRecorder.isRecording() || exportRunning.load()){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Stop audio first","Stop recording and playback before changing the audio interface.");
                return;
            }
            auto* selector=new juce::AudioDeviceSelectorComponent(
                deviceManager,0,8,1,2,false,false,false,false);
            selector->setSize(540,440);
            juce::DialogWindow::LaunchOptions options;
            options.dialogTitle="IDW Audio Devices - Select PreSonus AudioBox";
            options.content.setOwned(selector);
            options.dialogBackgroundColour=juce::Colours::darkgrey;
            options.escapeKeyTriggersCloseButton=true;
            options.useNativeTitleBar=true;
            options.resizable=false;
            options.launchAsync();
        };
        monitorControl.onClick=[this]{
            const bool enabled=monitorControl.getToggleState() && multiRecorder.isRecording();
            multiRecorder.setHeadphoneMonitor(enabled,0.1f);
            if(!multiRecorder.isRecording())
                monitorControl.setToggleState(false,juce::dontSendNotification);
        };
        armControl.setButtonText("ARM INPUT");addAndMakeVisible(armControl);
        // Tracks start disarmed; the user explicitly arms the intended destination.
        armedTracks.fill(false);
        armControl.onClick=[this]{
            if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load() ||
               selectedTrack>=session.tracks.size() ||
               session.tracks[selectedTrack].type!=idw::daw::TrackType::audio)return;
            // Treat our per-track arm state as authoritative; do not depend on the
            // button toggle state, which can be refreshed when selection changes.
            armedTracks[selectedTrack]=!armedTracks[selectedTrack];
            refreshMixer();
        };
        previousTrack.onClick=[this]{selectTrack(-1);};
        nextTrack.onClick=[this]{selectTrack(1);};
        masterLabel.setText("MASTER VOL",juce::dontSendNotification);
        addAndMakeVisible(masterLabel);
        masterGain.setRange(0.0,1.5,0.01);
        masterGain.setValue(1.0);
        masterGain.setTextBoxStyle(juce::Slider::TextBoxRight,false,55,25);
        addAndMakeVisible(masterGain);
        masterGain.onValueChange=[this]{masterLevel.store((float)masterGain.getValue(),std::memory_order_relaxed);};
        clearClips.setButtonText("RESET CLIP");addAndMakeVisible(clearClips);
        clearClips.onClick=[this]{peakClipLeft.store(false);peakClipRight.store(false);repaint();};
        eqLabel.setText("MASTER TONE",juce::dontSendNotification);addAndMakeVisible(eqLabel);
        lowEq.setRange(-12,12,0.5);lowEq.setValue(0);
        highEq.setRange(-12,12,0.5);highEq.setValue(0);
        for(auto* slider:{&lowEq,&highEq}){
            slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,56,24);
            addAndMakeVisible(*slider);
        }
        lowEq.onValueChange=[this]{lowDb.store((float)lowEq.getValue());};
        highEq.onValueChange=[this]{highDb.store((float)highEq.getValue());};
        limitLabel.setText("OUTPUT CEILING",juce::dontSendNotification);addAndMakeVisible(limitLabel);
        ceilingControl.setRange(-12.0,-0.1,0.1);ceilingControl.setValue(-1.0);
        ceilingControl.setTextBoxStyle(juce::Slider::TextBoxRight,false,60,24);
        addAndMakeVisible(ceilingControl);
        ceilingControl.onValueChange=[this]{ceilingDb.store((float)ceilingControl.getValue(),std::memory_order_relaxed);};
        refreshMixer();
    }
    void resized() override {
        auto area=getLocalBounds().reduced(20);
        setWantsKeyboardFocus(true);
        title.setBounds(area.removeFromTop(42));
        auto controls=area.removeFromTop(44);
        play.setBounds(controls.removeFromLeft(110).reduced(4));
        stop.setBounds(controls.removeFromLeft(100).reduced(4));
        tempoLabel.setBounds(controls.removeFromLeft(45));
        tempo.setBounds(controls.removeFromLeft(195));
        audio.setBounds(controls.removeFromLeft(160).reduced(3));
        midi.setBounds(controls.removeFromLeft(160).reduced(3));
        open.setBounds(controls.removeFromLeft(180).reduced(3));
        record.setBounds(controls.removeFromLeft(165).reduced(3));
        auto projectRow=area.removeFromTop(40);
        saveProject.setBounds(projectRow.removeFromLeft(145).reduced(3));
        openProject.setBounds(projectRow.removeFromLeft(145).reduced(3));
        exportProject.setBounds(projectRow.removeFromLeft(160).reduced(3));
        bounce.setBounds(projectRow.removeFromLeft(125).reduced(3));
        cancelBounce.setBounds(projectRow.removeFromLeft(135).reduced(3));
        connectButton.setBounds(projectRow.removeFromLeft(125).reduced(3));
        aiButton.setBounds(projectRow.removeFromLeft(125).reduced(3));
        pianoButton.setBounds(projectRow.removeFromLeft(125).reduced(3));
        synthPreset.setBounds(projectRow.removeFromLeft(100).reduced(3));
        auto mixRow=area.removeFromTop(42);
        gainLabel.setBounds(mixRow.removeFromLeft(110));
        gainControl.setBounds(mixRow.removeFromLeft(165));
        panLabel.setBounds(mixRow.removeFromLeft(45));
        panControl.setBounds(mixRow.removeFromLeft(165));
        muteControl.setBounds(mixRow.removeFromLeft(80));
        soloControl.setBounds(mixRow.removeFromLeft(75));
        previousTrack.setBounds(mixRow.removeFromLeft(100));
        nextTrack.setBounds(mixRow.removeFromLeft(100));
        armControl.setBounds(mixRow.removeFromLeft(105));
        inputChannel.setBounds(mixRow.removeFromLeft(90));
        auto monitorRow=area.removeFromTop(28);
        monitorControl.setBounds(monitorRow.removeFromLeft(190));
        audioDevices.setBounds(monitorRow.removeFromLeft(155).reduced(2));
        inputLevel.setBounds(area.removeFromTop(26));
        auto overdubRow=area.removeFromTop(36);
        overdubControl.setBounds(overdubRow.removeFromLeft(285));
        latencyLabel.setBounds(overdubRow.removeFromLeft(120));
        latencyAdjust.setBounds(overdubRow.removeFromLeft(200));
        analyzeCalibration.setBounds(overdubRow.removeFromLeft(200).reduced(3));
        generateCalibration.setBounds(overdubRow.removeFromLeft(170).reduced(3));
        liveCalibration.setBounds(overdubRow.removeFromLeft(175).reduced(3));
        auto masterRow=area.removeFromTop(38);
        masterLabel.setBounds(masterRow.removeFromLeft(115));
        masterGain.setBounds(masterRow.removeFromLeft(260));
        clearClips.setBounds(masterRow.removeFromLeft(120).reduced(3));
        auto synthRow=area.removeFromTop(35);
        attackLabel.setBounds(synthRow.removeFromLeft(100));
        attackControl.setBounds(synthRow.removeFromLeft(175));
        releaseLabel.setBounds(synthRow.removeFromLeft(115));
        releaseControl.setBounds(synthRow.removeFromLeft(175));
        auto eqRow=area.removeFromTop(35);
        eqLabel.setBounds(eqRow.removeFromLeft(115));
        lowEq.setBounds(eqRow.removeFromLeft(180));
        highEq.setBounds(eqRow.removeFromLeft(180));
        auto ceilingRow=area.removeFromTop(35);
        limitLabel.setBounds(ceilingRow.removeFromLeft(145));
        ceilingControl.setBounds(ceilingRow.removeFromLeft(220));
        connectPanel.setBounds(juce::Rectangle<int>(20,335,juce::jmin(getWidth()-40,565),205));
        aiPanel.setBounds(juce::Rectangle<int>(20,335,juce::jmin(getWidth()-40,575),225));
        pianoRoll.setBounds(juce::Rectangle<int>(20,335,juce::jmax(100,getWidth()-40),480));
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if(!selectedClipValid || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) ||
           exportRunning.load() || aiPanel.isVisible() || connectPanel.isVisible())
            return false;
        if(selectedClipTrack>=session.tracks.size() ||
           selectedClipIndex>=session.tracks[selectedClipTrack].clips.size()){
            selectedClipValid=false;
            return false;
        }
        const auto& clip=session.tracks[selectedClipTrack].clips[selectedClipIndex];
        const bool shift=key.getModifiers().isShiftDown();
        bool changed=false;
        if(key.getKeyCode()==juce::KeyPress::leftKey)
            changed=shift ? session.resizeClip(selectedClipTrack,selectedClipIndex,
                         std::max(0.25,clip.lengthBeats-0.25))
                          : session.moveClip(selectedClipTrack,selectedClipIndex,
                         std::max(0.0,clip.startBeat-0.25));
        else if(key.getKeyCode()==juce::KeyPress::rightKey)
            changed=shift ? session.resizeClip(selectedClipTrack,selectedClipIndex,
                         clip.lengthBeats+0.25)
                          : session.moveClip(selectedClipTrack,selectedClipIndex,
                         clip.startBeat+0.25);
        if(changed)repaint();
        return changed;
    }
    void mouseDown(const juce::MouseEvent& e) override {
        draggingClip=false;
        selectedClipValid=false;
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load() ||
           aiPanel.isVisible() || connectPanel.isVisible())return;
        // The Neo Mixer overview doubles as a channel selector when stopped.
        // Handle this before hit-testing clips beneath the overview.
        const int mixerTop=getHeight()-160;
        if(e.y>=mixerTop && e.y<mixerTop+112 && e.x>=30 && !session.tracks.empty()){
            const int count=juce::jmin(10,(int)session.tracks.size());
            const int width=juce::jmax(65,juce::jmin(120,(getWidth()-180)/juce::jmax(1,count+1)));
            const int index=(e.x-30)/width;
            if(index>=0 && index<count && e.x<30+(index+1)*width){
                selectedTrack=(size_t)index;
                refreshMixer();
                repaint();
            }
            return;
        }
        constexpr int top=366, height=70, left=205;
        if(e.y<top || e.x<left || session.tracks.empty())return;
        const auto ti=(size_t)((e.y-top)/height);
        if(ti>=session.tracks.size())return;
        const auto& clips=session.tracks[ti].clips;
        for(size_t i=clips.size();i>0;--i){
            const auto& c=clips[i-1];
            const int x=left+juce::roundToInt(c.startBeat*12.0);
            const int w=juce::jmax(2,juce::roundToInt(c.lengthBeats*12.0));
            if(e.x>=x && e.x<x+w && e.y>=top+(int)ti*height+8 &&
               e.y<top+(int)(ti+1)*height-10){
                dragTrack=ti;dragIndex=i-1;dragOriginX=e.x;
                selectedClipTrack=ti;selectedClipIndex=i-1;selectedClipValid=true;
                dragOriginalStart=c.startBeat;dragOriginalLength=c.lengthBeats;
                dragResize=e.x>=x+w-10;
                draggingClip=true;
                grabKeyboardFocus();
                repaint();
                break;
            }
        }
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if(!draggingClip || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) ||
           exportRunning.load())return;
        const auto delta=(double)(e.x-dragOriginX)/12.0;
        // Quarter-beat snapping on the visible timeline.
        const auto snap=[](double beats){return std::max(0.0,std::round(beats*4.0)/4.0);};
        if(dragResize)
            session.resizeClip(dragTrack,dragIndex,std::max(0.25,snap(dragOriginalLength+delta)));
        else
            session.moveClip(dragTrack,dragIndex,snap(dragOriginalStart+delta));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override { draggingClip=false; }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff080f1c));
        // Neo Studio control decks: visual grouping stays outside the audio callback.
        const auto deck=[&g](juce::Rectangle<float> bounds,const juce::String& label){
            g.setColour(juce::Colour(0xff101f32));
            g.fillRoundedRectangle(bounds,10.0f);
            g.setColour(juce::Colour(0xff224560));
            g.drawRoundedRectangle(bounds,10.0f,1.0f);
            g.setColour(juce::Colour(0xff47cffa));
            g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::bold)));
            g.drawText(label,bounds.removeFromTop(16.0f).reduced(10.0f,0.0f),juce::Justification::centredLeft);
        };
        const auto fullWidth=(float)juce::jmax(100,getWidth()-40);
        deck({20.0f,68.0f,fullWidth,93.0f},"TRANSPORT  /  PROJECT");
        deck({20.0f,164.0f,fullWidth,84.0f},"TRACK CONTROL  /  INPUT ROUTING");
        deck({20.0f,251.0f,fullWidth,74.0f},"MASTER  /  SYNTH  /  PROCESSING");
        const auto rect=getLocalBounds().reduced(20);
        const int top=330, trackHeight=70, left=205;
        g.setColour(juce::Colour(0xff122338));g.fillRect(20,top,getWidth()-40,36);
        g.setColour(juce::Colours::lightgrey);
        for(int bar=0;bar<17;++bar){
            const int x=left+bar*48;
            if(x>=getWidth()-30)break;
            g.drawVerticalLine(x,(float)top,(float)(getHeight()-30));
            g.drawText(juce::String(bar+1),x+3,top,40,30,juce::Justification::centredLeft);
        }
        int y=top+36;
        size_t renderedTrackIndex=0;
        for(const auto& track:session.tracks){
            g.setColour(juce::Colour((y/trackHeight)%2 ? 0xff16283d : 0xff102033));
            g.fillRect(20,y,getWidth()-40,trackHeight-2);
            const bool selected=renderedTrackIndex==selectedTrack;
            const bool armed=track.type==idw::daw::TrackType::audio &&
                renderedTrackIndex<armedTracks.size() && armedTracks[renderedTrackIndex];
            const auto laneAccent=armed ? juce::Colour(0xfff44368) :
                (track.type==idw::daw::TrackType::audio ? juce::Colour(0xff159cdb) : juce::Colour(0xffa66cff));
            g.setColour(laneAccent);
            g.fillRect(20,y,selected?6:4,trackHeight-2);
            if(selected){
                g.setColour(juce::Colour(0xff1bd5ff).withAlpha(0.75f));
                g.drawRect(20,y,getWidth()-40,trackHeight-2,1);
            }
            if(armed){
                g.setColour(juce::Colour(0xfff44368));
                g.fillEllipse(177.0f,(float)y+24.0f,12.0f,12.0f);
            }
            g.setColour(juce::Colours::white);
            g.drawText(juce::String(track.name),35,y+5,145,25,juce::Justification::centredLeft);
            // Compact Neo Studio mixer strip in each track header.
            const float stripX=36.0f,stripY=(float)y+45.0f,stripW=120.0f;
            g.setColour(juce::Colour(0xff304456));
            g.fillRoundedRectangle(stripX,stripY,stripW,5.0f,2.0f);
            g.setColour(laneAccent.withAlpha(0.9f));
            g.fillRoundedRectangle(stripX,stripY,stripW*juce::jlimit(0.0f,1.0f,track.gain/2.0f),5.0f,2.0f);
            g.setColour(juce::Colour(0xff94bbd1));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.drawText("VOL",36,y+32,37,12,juce::Justification::centredLeft);
            g.drawText(track.mute ? "MUTED" : (track.solo ? "SOLO" : (armed ? "REC READY" : "READY")),
                       83,y+32,100,12,juce::Justification::centredRight);
            size_t renderedClipIndex=0;
            for(const auto& clip:track.clips){
                const int clipX=left+juce::roundToInt(clip.startBeat*12.0);
                const int clipWidth=juce::jmax(2,juce::roundToInt(clip.lengthBeats*12.0));
                if(clipX>=getWidth()-25){++renderedClipIndex;continue;}
                g.setColour(track.type==idw::daw::TrackType::audio ? juce::Colour(0xff164f86) : juce::Colour(0xff513494));
                g.fillRoundedRectangle((float)clipX,(float)y+8,(float)juce::jmin(clipWidth,getWidth()-25-clipX), (float)trackHeight-18,4.0f);
                if(selectedClipValid && renderedTrackIndex==selectedClipTrack && renderedClipIndex==selectedClipIndex){
                    g.setColour(juce::Colour(0xff17d9ff));
                    g.drawRoundedRectangle((float)clipX,(float)y+8,(float)juce::jmin(clipWidth,getWidth()-25-clipX),(float)trackHeight-18,4.0f,2.0f);
                }
                ++renderedClipIndex;
                g.setColour(juce::Colours::white);
                g.drawFittedText(juce::File(juce::String(clip.sourcePath)).getFileName(),clipX+5,y+14,juce::jmax(0,juce::jmin(clipWidth-10,getWidth()-35-clipX)),25,juce::Justification::centredLeft,1);
            }
            y+=trackHeight;
            ++renderedTrackIndex;
            if(y>getHeight()-60)break;
        }
        const int head=left+(int)(playheadBeats*12.0);
        if(head<getWidth()-30){
            g.setColour(juce::Colour(0xff17d9ff));
            g.drawVerticalLine(head,(float)top,(float)(getHeight()-30));
        }
        // Peak bars are updated from atomics, never directly from the audio thread.
        const int meterLeft=getWidth()-220,meterTop=215,meterWidth=80;
        for(int ch=0;ch<2;++ch){
            const auto peak=ch==0?peakLeft.load():peakRight.load();
            const auto clipped=ch==0?peakClipLeft.load():peakClipRight.load();
            const int y=meterTop+ch*17;
            g.setColour(juce::Colours::darkgrey);g.fillRect(meterLeft,y,meterWidth,11);
            g.setColour(clipped?juce::Colour(0xfff44368):juce::Colour(0xff10d5f9));
            g.fillRect(meterLeft,y,juce::roundToInt(meterWidth*juce::jlimit(0.0f,1.0f,peak)),11);
            g.setColour(juce::Colours::white);g.drawText(ch==0?"L":"R",meterLeft-20,y,18,11,juce::Justification::centred);
        }
        // Compact bottom mixer overview. Display-only: no audio-thread mutation.
        const int mixHeight=112;
        const int mixTop=getHeight()-mixHeight-48;
        if(mixTop>top+90){
            const int count=juce::jmin(10,(int)session.tracks.size());
            const int stripWidth=juce::jmax(65,juce::jmin(120,(getWidth()-180)/juce::jmax(1,count+1)));
            g.setColour(juce::Colour(0xff101f32));
            g.fillRoundedRectangle(20.0f,(float)mixTop,(float)getWidth()-40.0f,(float)mixHeight,8.0f);
            g.setColour(juce::Colour(0xff47cffa));
            g.setFont(juce::Font(juce::FontOptions(11.0f,juce::Font::bold)));
            g.drawText("NEO MIXER  /  CLICK A CHANNEL TO SELECT",30,mixTop+4,320,18,juce::Justification::centredLeft);
            for(int i=0;i<count;++i){
                const auto& track=session.tracks[(size_t)i];
                if((size_t)i==selectedTrack){
                    g.setColour(juce::Colour(0xff16cefa).withAlpha(0.16f));
                    g.fillRoundedRectangle((float)(30+i*stripWidth), (float)mixTop+23,
                        (float)stripWidth-5, (float)mixHeight-29,5.0f);
                }
                const int x=30+i*stripWidth;
                g.setColour(juce::Colour(0xff29445c));
                g.drawVerticalLine(x+stripWidth-4,(float)mixTop+23,(float)mixTop+mixHeight-8);
                const bool armed=track.type==idw::daw::TrackType::audio && armedTracks[(size_t)i];
                g.setColour(armed ? juce::Colour(0xffef486d) : juce::Colour(0xffd2e8f5));
                g.setFont(juce::Font(juce::FontOptions(10.0f)));
                g.drawFittedText(juce::String(track.name),x+3,mixTop+27,stripWidth-14,17,juce::Justification::centredLeft,1);
                g.setColour(juce::Colour(0xff203447));
                g.fillRoundedRectangle((float)x+8,(float)mixTop+57,(float)stripWidth-26,7.0f,3.0f);
                g.setColour(armed ? juce::Colour(0xffef486d) : juce::Colour(0xff18bde8));
                g.fillRoundedRectangle((float)x+8,(float)mixTop+57,
                    (float)(stripWidth-26)*juce::jlimit(0.0f,1.0f,track.gain/2.0f),7.0f,3.0f);
                g.setColour(juce::Colour(0xff8cb6cb));
                g.drawText(track.mute ? "MUTE" : (track.solo ? "SOLO" : (armed ? "ARMED" : "CH "+juce::String(i+1))),
                    x+4,mixTop+75,stripWidth-14,20,juce::Justification::centredLeft);
            }
        }
        if(exportRunning.load()){
            g.setColour(juce::Colours::gold);
            g.drawText("Export: "+juce::String((int)(exportProgress.load()*100.0f))+"%",getWidth()-230,80,205,24,juce::Justification::centredRight);
        }
        g.setColour(juce::Colours::grey);
        g.drawFittedText(!sources.empty() ? "Loaded audio tracks: " + juce::String(loadedCount()) + "  |  Timeline playback prototype; drag clips to move or resize their right edge." : "Add an audio track and open WAV/AIFF files to mix their playback.",25,getHeight()-44,getWidth()-50,35,juce::Justification::centredLeft,1);
    }
private:
    bool draggingClip=false,dragResize=false;
    bool selectedClipValid=false;
    size_t selectedClipTrack=0,selectedClipIndex=0;
    size_t dragTrack=0,dragIndex=0;
    int dragOriginX=0;
    double dragOriginalStart=0.0,dragOriginalLength=0.0;
    void prepareToPlay(int samplesPerBlockExpected,double sampleRate) override {
        playing.store(false,std::memory_order_release);
        deviceRate=sampleRate;
        for(auto& state:eqState)state=0.0f;
        preparedBlockSize=juce::jmax(1,samplesPerBlockExpected);
        for(auto& slot:sources){
            slot->scratch.setSize(2,preparedBlockSize,false,true,false);
            slot->transport.prepareToPlay(samplesPerBlockExpected,sampleRate);
        }
    }
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        info.clearActiveBufferRegion();
        if(!playing.load(std::memory_order_relaxed) || deviceRate<=0)return;
        // Never resize heap buffers in the real-time audio callback.
        if(info.numSamples>preparedBlockSize || info.buffer->getNumChannels()>2)return;
        // Read a single session revision for the entire audio block.
        const auto snapshot=std::atomic_load_explicit(&playbackSession,std::memory_order_acquire);
        if(!snapshot)return;
        const auto& playback=*snapshot;
        const auto blockStart=sessionFrames.load(std::memory_order_relaxed);
        const bool anySolo=std::any_of(playback.tracks.begin(),playback.tracks.end(),[](const auto& t){return t.solo;});
        for(auto& slot:sources) {
            if(!slot->reader || slot->trackIndex>=playback.tracks.size())continue;
            const auto& track=playback.tracks[slot->trackIndex];
            if(track.mute || (anySolo && !track.solo) || track.clips.empty())continue;
            const auto& clip=track.clips.front();
            const auto window=idw::daw::overlap(blockStart,info.numSamples,deviceRate,
                    playback.bpm,clip.startBeat,clip.lengthBeats,clip.sourceOffsetSeconds);
            if(window.frames<=0)continue;
            slot->scratch.clear(0,window.frames);
            juce::AudioSourceChannelInfo readInfo(&slot->scratch,0,window.frames);
            slot->transport.getNextAudioBlock(readInfo);
            const float gain=juce::jlimit(0.0f,2.0f,track.gain);
            const float pan=juce::jlimit(-1.0f,1.0f,track.pan);
            const float leftGain=gain*(pan>0.0f?1.0f-pan:1.0f);
            const float rightGain=gain*(pan<0.0f?1.0f+pan:1.0f);
            for(int channel=0;channel<info.buffer->getNumChannels();++channel)
                info.buffer->addFrom(channel,info.startSample+window.offset,slot->scratch,channel,0,window.frames,channel==0?leftGain:rightGain);
        }
        // Basic audible piano-roll preview. No MIDI device or VST routing yet.
        if(info.buffer->getNumChannels()>0){
            idw::daw::renderMidiSine(playback,
                info.buffer->getWritePointer(0,info.startSample),
                info.buffer->getNumChannels()>1
                    ?info.buffer->getWritePointer(1,info.startSample):nullptr,
                info.numSamples,blockStart,deviceRate);
        }
        // Two-band master tone: one-pole low/high crossover at 500 Hz.
        // The filter's persistent state is owned exclusively by the audio callback.
        const float alpha=(float)std::exp(-2.0*juce::MathConstants<double>::pi*500.0/deviceRate);
        const float lowGain=std::pow(10.0f,lowDb.load(std::memory_order_relaxed)/20.0f);
        const float highGain=std::pow(10.0f,highDb.load(std::memory_order_relaxed)/20.0f);
        for(int channel=0;channel<juce::jmin(2,info.buffer->getNumChannels());++channel){
            auto* samples=info.buffer->getWritePointer(channel,info.startSample);
            float state=eqState[channel];
            for(int i=0;i<info.numSamples;++i){
                const float x=samples[i];
                state=alpha*state+(1.0f-alpha)*x;
                samples[i]=lowGain*state+highGain*(x-state);
            }
            eqState[channel]=state;
        }
        info.buffer->applyGain(info.startSample,info.numSamples,masterLevel.load(std::memory_order_relaxed));
        // Basic peak-ceiling safety clamp; not a look-ahead mastering limiter.
        const float ceiling=std::pow(10.0f,ceilingDb.load(std::memory_order_relaxed)/20.0f);
        for(int ch=0;ch<info.buffer->getNumChannels();++ch){
            auto* samples=info.buffer->getWritePointer(ch,info.startSample);
            for(int i=0;i<info.numSamples;++i)samples[i]=juce::jlimit(-ceiling,ceiling,samples[i]);
        }
        if(info.buffer->getNumChannels()>0){
            const float peak=info.buffer->getMagnitude(0,info.startSample,info.numSamples);
            peakLeft.store(peak,std::memory_order_relaxed);
            if(peak>=1.0f)peakClipLeft.store(true,std::memory_order_relaxed);
        }
        if(info.buffer->getNumChannels()>1){
            const float peak=info.buffer->getMagnitude(1,info.startSample,info.numSamples);
            peakRight.store(peak,std::memory_order_relaxed);
            if(peak>=1.0f)peakClipRight.store(true,std::memory_order_relaxed);
        }
        sessionFrames.fetch_add(info.numSamples,std::memory_order_relaxed);
    }
    void releaseResources() override {
        playing.store(false,std::memory_order_release);
        for(auto& slot:sources)slot->transport.releaseResources();
    }
    void refreshMixer(){
        if(session.tracks.empty())return;
        if(selectedTrack>=session.tracks.size())selectedTrack=0;
        const auto& track=session.tracks[selectedTrack];
        synthPreset.setEnabled(track.type==idw::daw::TrackType::midi &&
            !playing.load() && !(recorder.isRecording() || multiRecorder.isRecording()) && !exportRunning.load());
        synthPreset.setSelectedId(track.synthPreset+1,juce::dontSendNotification);
        const bool editableSynth=track.type==idw::daw::TrackType::midi &&
            !playing.load() && !(recorder.isRecording() || multiRecorder.isRecording()) && !exportRunning.load();
        attackControl.setEnabled(editableSynth);
        releaseControl.setEnabled(editableSynth);
        attackControl.setValue(track.synthAttackMs,juce::dontSendNotification);
        releaseControl.setValue(track.synthReleaseMs,juce::dontSendNotification);
        gainLabel.setText(juce::String(track.name)+" VOL",juce::dontSendNotification);
        gainControl.setValue(track.gain,juce::dontSendNotification);
        panControl.setValue(track.pan,juce::dontSendNotification);
        muteControl.setToggleState(track.mute,juce::dontSendNotification);
        soloControl.setToggleState(track.solo,juce::dontSendNotification);
        armControl.setEnabled(track.type==idw::daw::TrackType::audio &&
            !playing.load() && !(recorder.isRecording() || multiRecorder.isRecording()) && !exportRunning.load());
        const bool trackArmed=track.type==idw::daw::TrackType::audio && armedTracks[selectedTrack];
        armControl.setToggleState(trackArmed,juce::dontSendNotification);
        armControl.setButtonText(trackArmed ? "ARM INPUT - ON" : "ARM INPUT - OFF");
        armControl.setTooltip("Selected track: "+juce::String(track.name)+
            (trackArmed ? " (armed for recording)" : " (not armed). Click to arm before RECORD VOCAL."));
        inputChannel.setEnabled(track.type==idw::daw::TrackType::audio &&
            !playing.load() && !recorder.isRecording() &&
            !multiRecorder.isRecording() && !exportRunning.load());
        // Show the active device's physical channel names and enabled state.
        // Recording translates physical indices into JUCE's packed callback indices.
        if(auto* device=deviceManager.getCurrentAudioDevice()){
            const auto names=device->getInputChannelNames();
            const auto active=device->getActiveInputChannels();
            inputChannel.clear(juce::dontSendNotification);
            for(int i=0;i<names.size();++i){
                inputChannel.addItem(names[i]+(active[i] ? " (ON)" : " (disabled)"),i+1);
                inputChannel.setItemEnabled(i+1,active[i]);
            }
        }else{
            inputChannel.clear(juce::dontSendNotification);
            inputChannel.addItem("No audio input device",1);
            inputChannel.setItemEnabled(1,false);
        }
        inputChannel.setSelectedId(track.inputChannel+1,juce::dontSendNotification);
    }
    void selectTrack(int direction){
        if(playing.load() || session.tracks.empty())return;
        const auto current=(int)selectedTrack;
        selectedTrack=(size_t)juce::jlimit(0,(int)session.tracks.size()-1,current+direction);
        refreshMixer();repaint();
    }
    void updateFirstTrackMix(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load() || session.tracks.empty())return;
        session.setTrackMix(selectedTrack,(float)gainControl.getValue(),(float)panControl.getValue());
    }
    void timerCallback() override {
        if(loopbackCapture.consumeFailed()){
            liveCalibration.setEnabled(true);
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Live loopback interrupted",
                "Audio input or output became unavailable. The incomplete calibration was discarded.");
            if(!playing.load() && !recorder.isRecording() && !multiRecorder.isRecording())stopTimer();
            return;
        }
        if(loopbackCapture.consumeComplete()){
            const auto& captured=loopbackCapture.getCapturedBuffer();
            const double rate=loopbackCapture.getSampleRate();
            std::vector<std::size_t> pulses;
            for(const int ms:{100,200,300,400,500})
                pulses.push_back(static_cast<std::size_t>(std::llround(ms*rate/1000.0)));
            const auto alignment=idw::daw::estimateLoopbackLatency(
                captured.getReadPointer(0),static_cast<std::size_t>(loopbackCapture.recordedSamples()),
                pulses,static_cast<std::size_t>(std::llround(rate*0.04)));
            loopbackCapture.cancel();
            liveCalibration.setEnabled(true);
            const auto quality=idw::daw::assessCalibration(alignment,rate);
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                "Live loopback result",quality!=idw::daw::CalibrationQuality::invalid
                    ? "Measured "+juce::String(idw::daw::alignmentMilliseconds(alignment.lagSamples,rate),3)+
                      " ms, matched "+juce::String((int)alignment.accepted)+"/5 pulses. Jitter: "+
                      juce::String(idw::daw::alignmentMilliseconds(alignment.jitterSamples,rate),3)+
                      " ms. Quality: "+
                      juce::String(quality==idw::daw::CalibrationQuality::consistent?"consistent":"repeat test")+
                      ". Use TAKE TRIM ms manually after verifying recording alignment. No automatic adjustment was applied."
                    : "No reliable loopback measurement. Confirm line-level output-to-input routing.");
            if(!playing.load() && !recorder.isRecording() && !multiRecorder.isRecording())stopTimer();
            return;
        }
        if(multiRecorder.isRecording()){
            juce::String levels("RECORDING INPUTS  ");
            float maximum=0.0f;
            for(std::size_t i=0;i<multiTakes.size();++i){
                const float peak=multiRecorder.getInputPeak(i);
                maximum=std::max(maximum,peak);
                const double db=20.0*std::log10(std::max(1.0e-6f,peak));
                levels+="T"+juce::String((int)multiTakes[i].first+1)+": "+juce::String(db,0)+" dBFS  ";
            }
            inputLevel.setText(levels,juce::dontSendNotification);
            const double db=20.0*std::log10(std::max(1.0e-6f,maximum));
            record.setButtonText("REC PEAK "+juce::String(db,0)+" dBFS");
        }
        // Device shutdown can suspend playback from the audio thread.
        // Re-enable editing controls on the message thread after that transition.
        if(!playing.load() && !(recorder.isRecording() || multiRecorder.isRecording()) && !exportRunning.load()){
            for(auto& slot:sources)slot->transport.stop();
            refreshMixer();
            stopTimer();
            playheadBeats=0.0;
            repaint();
            return;
        }
        const auto activeSnapshot=std::atomic_load_explicit(&playbackSession,std::memory_order_acquire);
        const double timelineBpm=activeSnapshot?activeSnapshot->bpm:session.bpm;
        playheadBeats=idw::daw::playbackBeat(sessionFrames.load(),deviceRate,timelineBpm);
        if(playing.load() && !recorder.isRecording() && !multiRecorder.isRecording() &&
           activeSnapshot && playheadBeats>=idw::daw::arrangementEndBeat(*activeSnapshot)){
            playing.store(false);
            if(!exportRunning.load())stopTimer();
            for(auto& slot:sources)slot->transport.stop();
            refreshMixer();
        }
        repaint();
    }
    struct AudioSlot {
        juce::AudioTransportSource transport;
        std::unique_ptr<juce::AudioFormatReaderSource> reader;
        juce::File file;
        size_t trackIndex=0;
        double sourceOffsetSeconds=0.0;
        juce::AudioBuffer<float> scratch;
    };
    void addAudioTrack(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return; // Avoid mutating the audio graph during playback.
        if(!session.addTrack(idw::daw::TrackType::audio))return;
        auto slot=std::make_unique<AudioSlot>();
        slot->trackIndex=session.tracks.size()-1;
        if(preparedBlockSize>0){slot->scratch.setSize(2,preparedBlockSize,false,true,false);slot->transport.prepareToPlay(preparedBlockSize,deviceRate);}
        sources.push_back(std::move(slot));
        // Select the newly added track so ARM INPUT targets it immediately.
        // Do not arm it automatically: arming must remain an explicit user action.
        selectedTrack=session.tracks.size()-1;
        refreshMixer();
        repaint();
    }
    int loadedCount() const {
        int count=0;for(auto& slot:sources)if(slot->reader)++count;
        return count;
    }
    bool restoreProject(const juce::File& project){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return false;
        idw::daw::Session restored;
        if(!idw::daw::loadSession(project,restored))return false;
        // Build replacement players before replacing the current project.
        std::vector<std::unique_ptr<AudioSlot>> staged;
        int missing=0;
        for(size_t index=0;index<restored.tracks.size();++index){
            if(restored.tracks[index].type!=idw::daw::TrackType::audio)continue;
            auto slot=std::make_unique<AudioSlot>();
            slot->trackIndex=index;
            if(!restored.tracks[index].clips.empty()){
                const auto& clip=restored.tracks[index].clips.front();
                const auto savedPath=juce::String(clip.sourcePath);
                auto file=juce::File::isAbsolutePath(savedPath) ? juce::File(savedPath) : project.getParentDirectory().getChildFile(savedPath);
                // Portable-project fallback: look beside the project and in its Audio subfolder.
                if(!file.existsAsFile()){
                    const auto name=file.getFileName();
                    const auto adjacent=project.getParentDirectory().getChildFile(name);
                    const auto audioFolder=project.getParentDirectory().getChildFile("Audio").getChildFile(name);
                    if(adjacent.existsAsFile())file=adjacent;
                    else if(audioFolder.existsAsFile())file=audioFolder;
                }
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                if(reader && reader->sampleRate>0){
                    auto source=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true);
                    const double rate=source->getAudioFormatReader()->sampleRate;
                    slot->transport.setSource(source.get(),0,nullptr,rate);
                    slot->reader=std::move(source);
                    slot->file=file;
                    // Update restored reference so the next save uses the discovered location.
                    restored.tracks[index].clips.front().sourcePath=file.getFullPathName().toStdString();
                }else ++missing;
            }
            if(preparedBlockSize>0){
                slot->scratch.setSize(2,preparedBlockSize,false,true,false);
                slot->transport.prepareToPlay(preparedBlockSize,deviceRate);
            }
            staged.push_back(std::move(slot));
        }
        for(auto& slot:sources){slot->transport.stop();slot->transport.setSource(nullptr);}
        sources.clear();
        session=std::move(restored);
        sources=std::move(staged);
        armedTracks.fill(false); // Reopening a project must never arm recording automatically.
        selectedTrack=0;sessionFrames.store(0);playheadBeats=0;
        tempo.setValue(session.bpm,juce::dontSendNotification);
        refreshMixer();repaint();
        if(missing>0)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Missing audio",juce::String(missing)+" audio file(s) could not be located. Their clip references remain in the project.");
        return true;
    }
    void openProjectFile(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
        projectChooser=std::make_unique<juce::FileChooser>("Open IDW DAW Project",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.idwproj");
        projectChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& picker){
                const auto project=picker.getResult();
                if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
                if(project.existsAsFile() && !restoreProject(project))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Open failed",
                        "The project is invalid or cannot be opened during playback.");
            });
    }
    void bounceMix(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
        projectChooser=std::make_unique<juce::FileChooser>("Export stereo WAV",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IDW Mix.wav"),"*.wav");
        projectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& picker){
                auto output=picker.getResult();
                if(output==juce::File{} || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
                output=output.withFileExtension(".wav");
                if(exportThread.joinable())exportThread.join();
                const auto snapshot=session;
                const float gain=masterLevel.load(),ceiling=ceilingDb.load(),bass=lowDb.load(),treble=highDb.load();
                bounce.setEnabled(false);
                cancelBounce.setEnabled(true);
                exportCancel.store(false);
                exportProgress.store(0.0f);
                exportRunning.store(true);
                startTimerHz(10);
                const juce::Component::SafePointer<Workstation> safeThis(this);
                exportThread=std::thread([this,safeThis,snapshot,output,gain,ceiling,bass,treble]{
                    juce::String error;
                    const bool ok=idw::daw::renderSessionWav(snapshot,output,error,44100,gain,ceiling,bass,treble,&exportProgress,&exportCancel);
                    juce::MessageManager::callAsync([safeThis,ok,error]{
                        if(!safeThis)return;
                        safeThis->exportRunning.store(false);
                        safeThis->bounce.setEnabled(true);
                        safeThis->cancelBounce.setEnabled(false);
                        if(!safeThis->playing.load())safeThis->stopTimer();
                        safeThis->repaint();
                        juce::AlertWindow::showMessageBoxAsync(ok?juce::MessageBoxIconType::InfoIcon:juce::MessageBoxIconType::WarningIcon,
                            ok?"WAV export complete":"WAV bounce unavailable",ok?"Stereo WAV written successfully.":error);
                    });
                });
            });
    }
    void exportProjectFile(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
        projectChooser=std::make_unique<juce::FileChooser>("Export portable IDW Project",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*");
        projectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& picker){
                const auto folder=picker.getResult();
                if(folder==juce::File{} || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
                juce::String error;
                if(!idw::daw::exportPortableProject(session,folder,error))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Export failed",error);
            });
    }
    void saveProjectFile(){
        if(playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
        projectChooser=std::make_unique<juce::FileChooser>("Save IDW DAW Project",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IDW Session.idwproj"),"*.idwproj");
        projectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& picker){
                const auto file=picker.getResult();
                if(file==juce::File{} || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
                if(!idw::daw::saveSession(session,file.withFileExtension(".idwproj")))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Save failed","Could not write IDW project.");
            });
    }
    bool importTake(const juce::File& take){
        if(!take.existsAsFile() || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return false;
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(take));
        if(!reader || reader->sampleRate<=0 || reader->lengthInSamples<=0)return false;
        AudioSlot* target=nullptr;
        // Prefer the audio track selected when recording began, provided it
        // remains empty. Never overwrite a previously recorded/imported take.
        for(auto& slot:sources)
            if(slot->trackIndex==recordTargetTrack && !slot->reader){target=slot.get();break;}
        if(!target)
            for(auto& slot:sources)if(!slot->reader){target=slot.get();break;}
        if(!target){
            addAudioTrack();
            if(!sources.empty() && !sources.back()->reader)target=sources.back().get();
        }
        if(!target)return false;
        auto source=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true);
        const double rate=source->getAudioFormatReader()->sampleRate;
        const double duration=(double)source->getAudioFormatReader()->lengthInSamples/rate;
        // Compensate the input device's reported capture latency when importing
        // a new take. The initial delayed frames remain in the WAV master file.
        // This is driver-reported latency only, not measured round-trip calibration.
        const double adjustedLatency=takeInputLatencySeconds+takeManualTrimSeconds;
        const double trim=std::clamp(adjustedLatency,0.0,std::max(0.0,duration-1.0/rate));
        const double beatLength=(duration-trim)*session.bpm/60.0;
        if(!session.addClip(target->trackIndex,{take.getFullPathName().toStdString(),takeStartBeat,beatLength,trim}))return false;
        target->transport.setSource(source.get(),0,nullptr,rate);
        target->file=take;target->reader=std::move(source);
        selectedTrack=target->trackIndex;
        refreshMixer();repaint();return true;
    }
    void startLiveCalibration(){
        if(playing.load() || recorder.isRecording() || multiRecorder.isRecording() ||
           exportRunning.load() || loopbackCapture.isActive())return;
        // Input routing belongs to audio tracks; never index a missing track.
        if(selectedTrack>=session.tracks.size() ||
           session.tracks[selectedTrack].type!=idw::daw::TrackType::audio){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Live loopback unavailable","Select an audio track with an enabled line input.");
            return;
        }
        auto* device=deviceManager.getCurrentAudioDevice();
        const int input=session.tracks[selectedTrack].inputChannel;
        // JUCE callback channel arrays are packed by enabled hardware channels.
        // Translate the selected physical input index to its callback slot.
        const int callbackInput=device
            ? IDWLoopbackCapture::callbackIndexForChannel(device->getActiveInputChannels(),input)
            : -1;
        if(!device || input<0 || input>=device->getInputChannelNames().size() ||
           device->getOutputChannelNames().isEmpty() ||
           !device->getActiveInputChannels()[input] ||
           !device->getActiveOutputChannels()[0] ||
           !loopbackCapture.prepare(device->getCurrentSampleRate(),callbackInput,0) ||
           !loopbackCapture.start()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Live loopback unavailable","Select a track with an enabled line input and enable output channel 1.");
            return;
        }
        liveCalibration.setEnabled(false);
        startTimerHz(15);
    }
    void createCalibrationReference(){
        if(playing.load() || recorder.isRecording() || multiRecorder.isRecording() || exportRunning.load())return;
        referenceChooser=std::make_unique<juce::FileChooser>(
            "Save loopback test pulse WAV",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile("IDW Loopback Reference.wav"),"*.wav");
        referenceChooser->launchAsync(juce::FileBrowserComponent::saveMode |
            juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& picker){
                const auto file=picker.getResult();
                if(file==juce::File{} || playing.load() || recorder.isRecording() ||
                   multiRecorder.isRecording() || exportRunning.load())return;
                const auto destination=file.withFileExtension(".wav");
                // The generator never overwrites existing files, even if the
                // file chooser has confirmed replacement.
                if(destination.existsAsFile()){
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                        "File exists","Choose a new filename. Existing reference WAVs are preserved.");
                    return;
                }
                const bool ok=idw::daw::writeLoopbackReference(destination,48000.0,
                    {100.0,200.0,300.0,400.0,500.0});
                juce::AlertWindow::showMessageBoxAsync(
                    ok?juce::MessageBoxIconType::InfoIcon:juce::MessageBoxIconType::WarningIcon,
                    ok?"Reference WAV saved":"Reference WAV failed",
                    ok?"Saved 48 kHz pulse WAV. Expected pulse times: 100, 200, 300, 400, 500 ms.":
                       "Unable to create the reference WAV.");
            });
    }
    void analyzeCalibrationWav(){
        if(playing.load() || recorder.isRecording() || multiRecorder.isRecording() || exportRunning.load())return;
        auto* prompt=new juce::AlertWindow("Offline loopback WAV",
            "Enter known pulse times in milliseconds, separated by commas (minimum three). Analysis will not modify take trim.",
            juce::MessageBoxIconType::InfoIcon);
        prompt->addTextEditor("pulses","100,200,300,400,500","Expected pulse times (ms)");
        prompt->addTextEditor("channel","1","WAV channel (1-based)");
        prompt->addButton("Continue",1,juce::KeyPress(juce::KeyPress::returnKey));
        prompt->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
        prompt->enterModalState(true,juce::ModalCallbackFunction::create(
            [this,prompt](int result){
                if(result!=1){delete prompt;return;}
                const auto times=juce::StringArray::fromTokens(prompt->getTextEditorContents("pulses"),",","");
                const int channel=prompt->getTextEditorContents("channel").getIntValue()-1;
                delete prompt;
                continueCalibrationAnalysis(times,channel);
            }),false);
    }
    void continueCalibrationAnalysis(const juce::StringArray& times,int channel){
        if(times.size()<3 || times.size()>64 || channel<0 || channel>7){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Invalid calibration","Specify 3–64 pulse times and a valid WAV channel (1–8).");
            return;
        }
        std::vector<double> milliseconds;
        double previous=-1.0;
        for(const auto& token:times){
            const auto entry=token.trim();
            // getDoubleValue() alone accepts malformed entries such as "abc" as 0.
            bool numeric=!entry.isEmpty();
            int decimalPoints=0;
            for(const auto character:entry){
                if(character=='.')++decimalPoints;
                else if(character<'0' || character>'9')numeric=false;
            }
            if(decimalPoints>1 || entry==".")numeric=false;
            const double ms=numeric ? entry.getDoubleValue() : -1.0;
            if(!numeric || !std::isfinite(ms) || ms<=previous || ms<0 || ms>60000){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Invalid pulse schedule","Enter strictly increasing pulse times between 0 and 60000 ms.");
                return;
            }
            milliseconds.push_back(ms);previous=ms;
        }
        calibrationChooser=std::make_unique<juce::FileChooser>("Choose recorded loopback WAV",juce::File{},"*.wav");
        calibrationChooser->launchAsync(juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
            [this,milliseconds,channel](const juce::FileChooser& picker){
                const auto file=picker.getResult();
                if(!file.existsAsFile() || playing.load() || recorder.isRecording() ||
                   multiRecorder.isRecording() || exportRunning.load())return;
                juce::WavAudioFormat wav;
                auto reader=std::unique_ptr<juce::AudioFormatReader>(
                    wav.createReaderFor(file.createInputStream().release(),true));
                if(!reader || reader->sampleRate<=0 || reader->lengthInSamples>reader->sampleRate*60.0 ||
                   reader->lengthInSamples<=0){
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                        "Invalid WAV","Select a valid recorded WAV no longer than 60 seconds.");
                    return;
                }
                std::vector<std::size_t> positions;
                for(const auto ms:milliseconds)
                    positions.push_back(static_cast<std::size_t>(std::llround(ms*reader->sampleRate/1000.0)));
                const auto radius=static_cast<std::size_t>(std::llround(reader->sampleRate*0.04));
                const auto measured=idw::daw::analyzeLoopbackWav(file,channel,positions,radius);
                const auto message=measured.alignment.valid
                    ? "Measured pulse lag: "+juce::String(measured.alignment.lagSamples)+
                      " samples ("+juce::String(measured.lagMilliseconds,3)+" ms). Matched "+
                      juce::String((int)measured.alignment.accepted)+"/"+
                      juce::String((int)measured.alignment.total)+
                      " pulses. Maximum accepted timing deviation: "+
                      juce::String(measured.alignment.jitterSamples)+" samples ("+
                      juce::String(idw::daw::alignmentMilliseconds(measured.alignment.jitterSamples,measured.sampleRate),3)+
                      " ms). Quality: "+
                      juce::String(idw::daw::assessCalibration(measured.alignment,measured.sampleRate)==
                          idw::daw::CalibrationQuality::consistent ? "consistent" : "repeat measurement")+
                      ". No trim setting was changed."
                    : "Could not establish a consistent pulse offset. Confirm the channel, sample rate, pulse schedule and recording.";
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                    "Loopback analysis",message);
            });
    }
    void startOverdubPlayback(){
        const bool hasMaterial=loadedCount()>0 ||
            std::any_of(session.tracks.begin(),session.tracks.end(),
                [](const auto& t){return !t.notes.empty();});
        if(!hasMaterial)return;
        std::atomic_store_explicit(&playbackSession,
            std::make_shared<const idw::daw::Session>(session),std::memory_order_release);
        sessionFrames.store(0);
        for(auto& slot:sources){
            slot->transport.stop();
            slot->transport.setPosition(slot->sourceOffsetSeconds);
            slot->transport.start();
        }
        playing.store(true);
        startTimerHz(30);
    }
    void toggleRecording(){
        // Calibration owns a physical input/output while active. Do not let
        // recording start and contaminate either the pulse capture or a take.
        if(loopbackCapture.isActive()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Calibration running","Finish the live loopback test before starting a recording.");
            return;
        }
        if(exportRunning.load()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Recording unavailable","Finish exporting before starting a recording.");
            return;
        }
        // Recording may stop while the transport runs, but must not start
        // while ordinary playback is active. Explain this instead of silently ignoring RECORD.
        if(!recorder.isRecording() && !multiRecorder.isRecording() && playing.load()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Stop playback first",
                "Press STOP, arm an empty audio track, enable PLAY BEAT WHILE RECORDING, then press RECORD VOCAL. Do not press PLAY first.");
            return;
        }
        if((recorder.isRecording() || multiRecorder.isRecording())){
            const bool wasMulti=multiRecorder.isRecording();
            multiRecorder.setHeadphoneMonitor(false);
            monitorControl.setToggleState(false,juce::dontSendNotification);
            recorder.stop();multiRecorder.stop();record.setButtonText("RECORD VOCAL");
            monitorControl.setEnabled(false);
            inputLevel.setText("INPUT LEVEL: idle",juce::dontSendNotification);
            const auto dropped=wasMulti ? multiRecorder.getDroppedBlocks() : recorder.getDroppedBlocks();
            const auto rejected=wasMulti ? multiRecorder.getRejectedWrites() : recorder.getRejectedWrites();
            const auto receivedFrames=wasMulti ? std::uint64_t{0} : recorder.getCapturedFrames();
            const auto queuedFrames=wasMulti ? std::uint64_t{0} : recorder.getQueuedFrames();
            // Playback and capture counters currently run in separate callbacks;
            // this discrepancy is diagnostic, not an automatic alignment offset.
            const bool wasOverdubbing=playing.load() && takeWasOverdub;
            const auto playbackFrames=static_cast<std::uint64_t>(
                std::max<std::int64_t>(0,sessionFrames.load(std::memory_order_relaxed)));
            // Stop the transport before changing the audio graph.
            for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(0);}
            playing.store(false);stopTimer();sessionFrames.store(0);playheadBeats=0;
            if(wasMulti){
                juce::String failures;
                for(const auto& item:multiTakes){
                    recordTargetTrack=item.first;
                    if(!importTake(item.second))failures+=item.second.getFileName()+" ";
                }
                multiTakes.clear();
                if(rejected>0)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Audio disk buffer overflow",
                    juce::String(rejected)+" WAV write operations were rejected. Check recorded takes for missing audio.");
                if(dropped>0)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Recording may be incomplete",
                    juce::String(dropped)+" audio input blocks were skipped. Check recording synchronization.");
                if(failures.isNotEmpty())juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Multitrack import incomplete",failures);
            }else{
                if(wasOverdubbing && playbackFrames>0 && receivedFrames>0 && dropped==0 && rejected==0){
                    const auto difference=static_cast<juce::int64>(playbackFrames)-
                        static_cast<juce::int64>(receivedFrames);
                    juce::Logger::writeToLog("IDW overdub timing diagnostic: playback frames="+
                        juce::String(static_cast<juce::int64>(playbackFrames))+
                        ", captured frames="+juce::String(static_cast<juce::int64>(receivedFrames))+
                        ", frame difference="+juce::String(difference)+
                        " (callback counters, not a latency correction)");
                }
                if(queuedFrames<receivedFrames)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Vocal recording frame mismatch",
                    juce::String(static_cast<juce::int64>(receivedFrames-queuedFrames))+
                    " captured audio frames were not accepted by the WAV queue. Inspect take timing.");
                if(rejected>0)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Vocal WAV buffer overflow",
                    juce::String(rejected)+" single-take WAV writes were rejected. Inspect the saved recording.");
                if(dropped>0)juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,"Vocal capture interrupted",
                    juce::String(dropped)+" audio input blocks were skipped. Check vocal timing.");
                const auto take=recorder.file();
                if(!importTake(take))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                        "Take saved, import failed",take.getFullPathName());
            }
            return;
        }
        std::vector<std::size_t> selectedInputs;
        for(std::size_t i=0;i<session.tracks.size();++i)
            if(session.tracks[i].type==idw::daw::TrackType::audio && armedTracks[i])
                selectedInputs.push_back(i);
        recordTargetTrack=selectedInputs.empty()?session.tracks.size():selectedInputs.front();
        if(recordTargetTrack>=session.tracks.size()){
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "No armed audio track",
                "No audio track is armed. Currently selected: "+
                (selectedTrack<session.tracks.size()
                    ? juce::String(session.tracks[selectedTrack].name)
                    : juce::String("none"))+
                ". Press STOP, select an empty AUDIO track, and click ARM INPUT until "
                "it reads ARM INPUT - ON. Then press RECORD VOCAL without pressing PLAY.");
            return;
        }
        const auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("IDW DAW").getChildFile("Recordings");
        folder.createDirectory();
        auto destination=folder.getNonexistentChildFile("Vocal Take",".wav",false);
        // Use driver-reported input latency to trim delayed capture frames
        // at take import. Real loopback calibration remains future work.
        takeInputLatencySeconds=0.0;
        takeManualTrimSeconds=latencyAdjust.getValue()/1000.0;
        if(auto* device=deviceManager.getCurrentAudioDevice())
            if(deviceRate>0 && std::isfinite(deviceRate))
                takeInputLatencySeconds=(double)std::max(0,device->getInputLatencyInSamples())/deviceRate;
        takeWasOverdub=overdubControl.getToggleState();
        takeStartBeat=deviceRate>0 ? (double)sessionFrames.load()*session.bpm/(deviceRate*60.0) : 0.0;
        monitorControl.setToggleState(false,juce::dontSendNotification);
        multiRecorder.setHeadphoneMonitor(false);
        if(overdubControl.getToggleState()){
            const bool hasPlaybackMaterial=loadedCount()>0 ||
                std::any_of(session.tracks.begin(),session.tracks.end(),
                    [](const auto& track){return !track.notes.empty();});
            if(!hasPlaybackMaterial){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Overdub unavailable","Load an audio clip or add MIDI notes before recording over playback.");
                return;
            }
            // The recording writer starts before transport playback. Reset the
            // reference beat to the beginning of the playback arrangement.
            // Physical capture/playback alignment still needs loopback QA.
            takeStartBeat=0.0;
            // Overdubs must use empty destination tracks. Existing material
            // is preserved and never overwritten by the recording operation.
            for(const auto index:selectedInputs)
                for(const auto& slot:sources)
                    if(slot->trackIndex==index && slot->reader){
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,"Overdub needs empty tracks",
                            "Add and arm an empty audio track to record alongside playback.");
                        return;
                    }
        }
        if(selectedInputs.size()>1){
            // A loaded track cannot accept a second AudioSlot reader yet.
            for(const auto index:selectedInputs)
                for(const auto& slot:sources)
                    if(slot->trackIndex==index && slot->reader){
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,"Track already has audio",
                            "Arm empty audio tracks for simultaneous recording.");
                        return;
                    }
            auto* device=deviceManager.getCurrentAudioDevice();
            const auto active=device ? device->getActiveInputChannels() : juce::BigInteger();
            bool validInputs=device!=nullptr;
            for(std::size_t i=0;i<selectedInputs.size();++i){
                const int channel=session.tracks[selectedInputs[i]].inputChannel;
                if(channel<0 || (device && channel>=device->getInputChannelNames().size()) ||
                   IDWLoopbackCapture::callbackIndexForChannel(active,channel)<0)validInputs=false;
                for(std::size_t j=0;j<i;++j)
                    if(session.tracks[selectedInputs[j]].inputChannel==channel)validInputs=false;
            }
            if(selectedInputs.size()>IDWMultiTakeRecorder::maxTracks ||
               !validInputs || !std::isfinite(deviceRate) || deviceRate<=0.0){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "More inputs needed","Enable one physical input per armed audio track in audio device settings.");
                return;
            }
            std::vector<IDWMultiTakeRecorder::Input> inputs;
            multiTakes.clear();
            for(std::size_t i=0;i<selectedInputs.size();++i){
                const auto path=folder.getNonexistentChildFile(
                    "Take "+juce::String(selectedInputs[i]+1)+" "+juce::String(juce::Time::getCurrentTime().toMilliseconds()),".wav",false);
                inputs.push_back({path,IDWLoopbackCapture::callbackIndexForChannel(active,
                    session.tracks[selectedInputs[i]].inputChannel)});
                multiTakes.emplace_back(selectedInputs[i],path);
            }
            if(multiRecorder.start(inputs,deviceRate)){
                record.setButtonText("STOP RECORD");
                monitorControl.setEnabled(true);
                if(overdubControl.getToggleState())startOverdubPlayback();
                startTimerHz(15); // UI peak updates only; never poll on the audio callback.
            }
            else{
                multiTakes.clear();
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Multitrack unavailable","Could not start all armed inputs.");
            }
        }else{
            monitorControl.setEnabled(false);
            auto* device=deviceManager.getCurrentAudioDevice();
            const int physicalInput=session.tracks[recordTargetTrack].inputChannel;
            const int callbackInput=device
                ? IDWLoopbackCapture::callbackIndexForChannel(device->getActiveInputChannels(),physicalInput)
                : -1;
            if(!device || physicalInput<0 ||
               physicalInput>=device->getInputChannelNames().size() || callbackInput<0 ||
               !std::isfinite(deviceRate) || deviceRate<=0.0){
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Input unavailable","Select an existing enabled input in audio device settings.");
                return;
            }
            if(recorder.start(destination,deviceRate,1,callbackInput)){
                record.setButtonText("STOP RECORD");
                if(overdubControl.getToggleState())startOverdubPlayback();
            }
            else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Recording unavailable","Check microphone permission, device input and destination directory.");
        }
    }
    void loadAudio(){
        chooser=std::make_unique<juce::FileChooser>("Select WAV or AIFF audio file",juce::File{},"*.wav;*.aiff;*.aif");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser& picker){
            auto file=picker.getResult();if(!file.existsAsFile() || playing.load() || (recorder.isRecording() || multiRecorder.isRecording()) || exportRunning.load())return;
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
            if(!reader)return;
            AudioSlot* target=nullptr;
            for(auto& slot:sources)if(!slot->reader){target=slot.get();break;}
            if(!target)return; // Add another audio track before importing another file.
            auto source=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true);
            const auto sourceRate=source->getAudioFormatReader()->sampleRate;
            target->transport.setSource(source.get(),0,nullptr,sourceRate);
            target->file=file;target->reader=std::move(source);
            session.addClip(target->trackIndex,{file.getFullPathName().toStdString(),0.0,
                sourceRate>0 ? (double)target->reader->getAudioFormatReader()->lengthInSamples/sourceRate*session.bpm/60.0 : 0.0,0.0});
            sessionFrames.store(0);playing=false;playheadBeats=0;repaint();
        });
    }
    ~Workstation() override {setLookAndFeel(nullptr);if(exportThread.joinable())exportThread.join();recorder.stop();multiRecorder.stop();loopbackCapture.cancel();deviceManager.removeAudioCallback(&loopbackCapture);deviceManager.removeAudioCallback(&multiRecorder);deviceManager.removeAudioCallback(&recorder);shutdownAudio();for(auto& slot:sources)slot->transport.setSource(nullptr);sources.clear();}
    IDWNeoLookAndFeel neoTheme;
    std::thread exportThread;
    std::atomic<bool> exportRunning{false};
    std::atomic<bool> exportCancel{false};
    std::atomic<float> exportProgress{0.0f};
    idw::daw::Session session;
    std::atomic<bool> playing{false};
    double playheadBeats=0;
    double takeStartBeat=0.0;
    bool takeWasOverdub=false;
    size_t recordTargetTrack=0;
    double takeInputLatencySeconds=0.0;
    double takeManualTrimSeconds=0.0;
    juce::Label title,tempoLabel,gainLabel,panLabel,inputLevel,latencyLabel;
    juce::Slider gainControl,panControl,masterGain,latencyAdjust;
    juce::Label masterLabel,eqLabel,limitLabel;
    juce::Slider ceilingControl;
    std::atomic<float> ceilingDb{-1.0f};
    juce::Slider lowEq,highEq;
    std::atomic<float> lowDb{0.0f},highDb{0.0f};
    float eqState[2]{0.0f,0.0f};
    juce::TextButton clearClips,audioDevices;
    std::atomic<float> peakLeft{0.0f},peakRight{0.0f};
    std::atomic<bool> peakClipLeft{false},peakClipRight{false};
    std::atomic<float> masterLevel{1.0f};
    juce::ToggleButton muteControl,soloControl,armControl,monitorControl,overdubControl;
    juce::ComboBox inputChannel;
    std::array<bool,128> armedTracks{}; // Temporary UI state, not persisted in projects.
    juce::TextButton previousTrack,nextTrack;
    size_t selectedTrack=0;
    juce::TextButton play,stop,audio,midi,open,record,saveProject,openProject,exportProject,bounce,cancelBounce,connectButton,aiButton,pianoButton,analyzeCalibration,generateCalibration,liveCalibration;
    juce::ComboBox synthPreset;
    juce::Label attackLabel,releaseLabel;
    juce::Slider attackControl,releaseControl;
    IDWPianoRoll pianoRoll;
    IDWAIApprovalPanel aiPanel;
    IDWConnectPanel connectPanel;
    IDWTakeRecorder recorder;
    IDWMultiTakeRecorder multiRecorder;
    IDWLoopbackCapture loopbackCapture;
    std::vector<std::pair<std::size_t,juce::File>> multiTakes;
    juce::AudioFormatManager formats;
    std::atomic<int64_t> sessionFrames{0};
    // Allocated on the message thread before transport starts; audio only reads.
    std::shared_ptr<const idw::daw::Session> playbackSession;
    double deviceRate=48000.0;
    int preparedBlockSize=0;
    std::vector<std::unique_ptr<AudioSlot>> sources;
    std::unique_ptr<juce::FileChooser> chooser,projectChooser,calibrationChooser,referenceChooser;
    juce::Slider tempo;
};
class DAWApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override {return "IDW DAW V14 Foundation";}
    const juce::String getApplicationVersion() override {return "14.0.0-dev";}
    void initialise(const juce::String&) override {
        window=std::make_unique<MainWindow>(getApplicationName());
    }
    void shutdown() override {window.reset();}
private:
    class MainWindow final : public juce::DocumentWindow {
    public:
        MainWindow(juce::String name):DocumentWindow(name,juce::Colours::black,allButtons){
            setUsingNativeTitleBar(true);
            setContentOwned(new Workstation(),true);
            centreWithSize(getWidth(),getHeight());
            setVisible(true);
        }
        void closeButtonPressed() override {juce::JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<MainWindow> window;
};
START_JUCE_APPLICATION(DAWApplication)
