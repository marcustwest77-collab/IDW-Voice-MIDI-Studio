#include "MidiSineSynth.h"
#include "PianoRoll.h"
#include <JuceHeader.h>
#include "SessionModel.h"
#include "TimelineMath.h"
#include "TakeRecorder.h"
#include "SessionPersistence.h"
#include "ProjectPackaging.h"
#include "OfflineBounce.h"
#include "IDWConnectPanel.h"
#include "AIApprovalPanel.h"
#include "AIStatusSnapshot.h"
#include <atomic>
#include <thread>
class Workstation final : public juce::AudioAppComponent, private juce::Timer {
public:
    Workstation() {
        setSize(1100,700);
        formats.registerBasicFormats();
        setAudioChannels(1,2);
        deviceManager.addAudioCallback(&recorder);
        title.setText("IDW  |  VOICE MIDI STUDIO DAW  |  V14 FOUNDATION",juce::dontSendNotification);
        title.setColour(juce::Label::textColourId,juce::Colours::gold);
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
        synthPreset.setSelectedId(1,juce::dontSendNotification);
        addAndMakeVisible(synthPreset);
        synthPreset.onChange=[this]{
            if(playing.load() || selectedTrack>=session.tracks.size())return;
            session.setTrackSynthPreset(selectedTrack,synthPreset.getSelectedId()-1);
        };
        pianoButton.setButtonText("PIANO ROLL");
        addAndMakeVisible(pianoButton);
        addChildComponent(pianoRoll);
        pianoRoll.session=&session;
        pianoButton.onClick=[this]{
            if(playing.load() || recorder.isRecording() || exportRunning.load())return;
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
                session,playing.load(),recorder.isRecording()),true);
        };
        aiPanel.onApproved=[this](const idw::daw::AIProposal& proposal){
            if(playing.load() || recorder.isRecording() || exportRunning.load())return false;
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
        audio.setButtonText("+ AUDIO TRACK"); midi.setButtonText("+ MIDI TRACK");
        play.onClick=[this]{if(loadedCount()>0 || std::any_of(session.tracks.begin(),session.tracks.end(),[](const auto& t){return !t.notes.empty();})){sessionFrames.store(0);for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(slot->sourceOffsetSeconds);slot->transport.start();}playing=true;startTimerHz(30);repaint();}};
        stop.onClick=[this]{if(recorder.isRecording())toggleRecording();for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(slot->sourceOffsetSeconds);}sessionFrames.store(0);playing=false;stopTimer();playheadBeats=0;repaint();};
        open.onClick=[this]{loadAudio();};
        record.onClick=[this]{toggleRecording();};
        saveProject.onClick=[this]{saveProjectFile();};
        openProject.onClick=[this]{openProjectFile();};
        exportProject.onClick=[this]{exportProjectFile();};
        bounce.onClick=[this]{bounceMix();};
        cancelBounce.onClick=[this]{exportCancel.store(true);};
        audio.onClick=[this]{addAudioTrack();refreshMixer();repaint();};
        midi.onClick=[this]{if(!playing.load())session.addTrack(idw::daw::TrackType::midi);refreshMixer();repaint();};
        tempo.setRange(20,300,1);tempo.setValue(120);tempo.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,28);
        tempo.onValueChange=[this]{session.setTempo(tempo.getValue());};addAndMakeVisible(tempo);
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
        play.setBounds(controls.removeFromLeft(90).reduced(3));
        stop.setBounds(controls.removeFromLeft(90).reduced(3));
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
        gainControl.setBounds(mixRow.removeFromLeft(185));
        panLabel.setBounds(mixRow.removeFromLeft(45));
        panControl.setBounds(mixRow.removeFromLeft(185));
        muteControl.setBounds(mixRow.removeFromLeft(80));
        soloControl.setBounds(mixRow.removeFromLeft(75));
        previousTrack.setBounds(mixRow.removeFromLeft(100));
        nextTrack.setBounds(mixRow.removeFromLeft(100));
        auto masterRow=area.removeFromTop(38);
        masterLabel.setBounds(masterRow.removeFromLeft(115));
        masterGain.setBounds(masterRow.removeFromLeft(260));
        clearClips.setBounds(masterRow.removeFromLeft(120).reduced(3));
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
        if(!selectedClipValid || playing.load() || recorder.isRecording() ||
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
        if(playing.load() || recorder.isRecording() || exportRunning.load() ||
           aiPanel.isVisible() || connectPanel.isVisible())return;
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
        if(!draggingClip || playing.load() || recorder.isRecording() ||
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
        g.fillAll(juce::Colour(0xff101318));
        const auto rect=getLocalBounds().reduced(20);
        const int top=330, trackHeight=70, left=205;
        g.setColour(juce::Colour(0xff1b222b));g.fillRect(20,top,getWidth()-40,36);
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
            g.setColour(juce::Colour((y/trackHeight)%2 ? 0xff252b35 : 0xff1c222a));
            g.fillRect(20,y,getWidth()-40,trackHeight-2);
            g.setColour(track.type==idw::daw::TrackType::audio ? juce::Colours::orange : juce::Colours::deepskyblue);
            g.fillRect(20,y,5,trackHeight-2);
            g.setColour(juce::Colours::white);
            g.drawText(juce::String(track.name),35,y+10,155,35,juce::Justification::centredLeft);
            size_t renderedClipIndex=0;
            for(const auto& clip:track.clips){
                const int clipX=left+juce::roundToInt(clip.startBeat*12.0);
                const int clipWidth=juce::jmax(2,juce::roundToInt(clip.lengthBeats*12.0));
                if(clipX>=getWidth()-25){++renderedClipIndex;continue;}
                g.setColour(juce::Colour(0xff89551e));
                g.fillRoundedRectangle((float)clipX,(float)y+8,(float)juce::jmin(clipWidth,getWidth()-25-clipX), (float)trackHeight-18,4.0f);
                if(selectedClipValid && renderedTrackIndex==selectedClipTrack && renderedClipIndex==selectedClipIndex){
                    g.setColour(juce::Colours::gold);
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
            g.setColour(juce::Colours::gold);
            g.drawVerticalLine(head,(float)top,(float)(getHeight()-30));
        }
        // Peak bars are updated from atomics, never directly from the audio thread.
        const int meterLeft=getWidth()-220,meterTop=215,meterWidth=80;
        for(int ch=0;ch<2;++ch){
            const auto peak=ch==0?peakLeft.load():peakRight.load();
            const auto clipped=ch==0?peakClipLeft.load():peakClipRight.load();
            const int y=meterTop+ch*17;
            g.setColour(juce::Colours::darkgrey);g.fillRect(meterLeft,y,meterWidth,11);
            g.setColour(clipped?juce::Colours::red:juce::Colours::limegreen);
            g.fillRect(meterLeft,y,juce::roundToInt(meterWidth*juce::jlimit(0.0f,1.0f,peak)),11);
            g.setColour(juce::Colours::white);g.drawText(ch==0?"L":"R",meterLeft-20,y,18,11,juce::Justification::centred);
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
        const auto blockStart=sessionFrames.load(std::memory_order_relaxed);
        const bool anySolo=std::any_of(session.tracks.begin(),session.tracks.end(),[](const auto& t){return t.solo;});
        for(auto& slot:sources) {
            if(!slot->reader || slot->trackIndex>=session.tracks.size())continue;
            const auto& track=session.tracks[slot->trackIndex];
            if(track.mute || (anySolo && !track.solo) || track.clips.empty())continue;
            const auto& clip=track.clips.front();
            const auto window=idw::daw::overlap(blockStart,info.numSamples,deviceRate,
                    session.bpm,clip.startBeat,clip.lengthBeats,clip.sourceOffsetSeconds);
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
            idw::daw::renderMidiSine(session,
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
        for(auto& slot:sources)slot->transport.releaseResources();
    }
    void refreshMixer(){
        if(session.tracks.empty())return;
        if(selectedTrack>=session.tracks.size())selectedTrack=0;
        const auto& track=session.tracks[selectedTrack];
        synthPreset.setEnabled(track.type==idw::daw::TrackType::midi);
        synthPreset.setSelectedId(track.synthPreset+1,juce::dontSendNotification);
        gainLabel.setText(juce::String(track.name)+" VOL",juce::dontSendNotification);
        gainControl.setValue(track.gain,juce::dontSendNotification);
        panControl.setValue(track.pan,juce::dontSendNotification);
        muteControl.setToggleState(track.mute,juce::dontSendNotification);
        soloControl.setToggleState(track.solo,juce::dontSendNotification);
    }
    void selectTrack(int direction){
        if(playing.load() || session.tracks.empty())return;
        const auto current=(int)selectedTrack;
        selectedTrack=(size_t)juce::jlimit(0,(int)session.tracks.size()-1,current+direction);
        refreshMixer();repaint();
    }
    void updateFirstTrackMix(){
        if(playing.load() || session.tracks.empty())return;
        session.setTrackMix(selectedTrack,(float)gainControl.getValue(),(float)panControl.getValue());
    }
    void timerCallback() override {
        playheadBeats=deviceRate>0?(double)sessionFrames.load()*session.bpm/(deviceRate*60.0):0.0;
        if(playing && playheadBeats>=lastClipEndBeat()){playing=false;if(!exportRunning.load())stopTimer();for(auto& slot:sources)slot->transport.stop();}
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
        if(playing.load())return; // Avoid mutating the audio graph during playback.
        if(!session.addTrack(idw::daw::TrackType::audio))return;
        auto slot=std::make_unique<AudioSlot>();
        slot->trackIndex=session.tracks.size()-1;
        if(preparedBlockSize>0){slot->scratch.setSize(2,preparedBlockSize,false,true,false);slot->transport.prepareToPlay(preparedBlockSize,deviceRate);}
        sources.push_back(std::move(slot));
    }
    double lastClipEndBeat() const {double end=0;for(const auto& track:session.tracks){for(const auto& clip:track.clips)end=juce::jmax(end,clip.startBeat+clip.lengthBeats);for(const auto& note:track.notes)end=juce::jmax(end,note.startBeat+note.lengthBeats);}return end;}
    int loadedCount() const {
        int count=0;for(auto& slot:sources)if(slot->reader)++count;
        return count;
    }
    bool restoreProject(const juce::File& project){
        if(playing.load() || recorder.isRecording())return false;
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
        selectedTrack=0;sessionFrames.store(0);playheadBeats=0;
        tempo.setValue(session.bpm,juce::dontSendNotification);
        refreshMixer();repaint();
        if(missing>0)juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "Missing audio",juce::String(missing)+" audio file(s) could not be located. Their clip references remain in the project.");
        return true;
    }
    void openProjectFile(){
        if(playing.load() || recorder.isRecording())return;
        projectChooser=std::make_unique<juce::FileChooser>("Open IDW DAW Project",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*.idwproj");
        projectChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& picker){
                const auto project=picker.getResult();
                if(project.existsAsFile() && !restoreProject(project))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Open failed",
                        "The project is invalid or cannot be opened during playback.");
            });
    }
    void bounceMix(){
        if(playing.load() || recorder.isRecording())return;
        projectChooser=std::make_unique<juce::FileChooser>("Export stereo WAV",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IDW Mix.wav"),"*.wav");
        projectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& picker){
                auto output=picker.getResult();if(output==juce::File{})return;
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
        if(playing.load() || recorder.isRecording())return;
        projectChooser=std::make_unique<juce::FileChooser>("Export portable IDW Project",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),"*");
        projectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& picker){
                const auto folder=picker.getResult();
                if(folder==juce::File{})return;
                juce::String error;
                if(!idw::daw::exportPortableProject(session,folder,error))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Export failed",error);
            });
    }
    void saveProjectFile(){
        if(recorder.isRecording())return;
        projectChooser=std::make_unique<juce::FileChooser>("Save IDW DAW Project",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("IDW Session.idwproj"),"*.idwproj");
        projectChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& picker){
                const auto file=picker.getResult();
                if(file==juce::File{})return;
                if(!idw::daw::saveSession(session,file.withFileExtension(".idwproj")))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Save failed","Could not write IDW project.");
            });
    }
    bool importTake(const juce::File& take){
        if(!take.existsAsFile() || playing.load())return false;
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(take));
        if(!reader || reader->sampleRate<=0 || reader->lengthInSamples<=0)return false;
        AudioSlot* target=nullptr;
        for(auto& slot:sources)if(!slot->reader){target=slot.get();break;}
        if(!target){
            addAudioTrack();
            if(!sources.empty() && !sources.back()->reader)target=sources.back().get();
        }
        if(!target)return false;
        auto source=std::make_unique<juce::AudioFormatReaderSource>(reader.release(),true);
        const double rate=source->getAudioFormatReader()->sampleRate;
        const double duration=(double)source->getAudioFormatReader()->lengthInSamples/rate;
        const double beatLength=duration*session.bpm/60.0;
        if(!session.addClip(target->trackIndex,{take.getFullPathName().toStdString(),takeStartBeat,beatLength,0.0}))return false;
        target->transport.setSource(source.get(),0,nullptr,rate);
        target->file=take;target->reader=std::move(source);
        repaint();return true;
    }
    void toggleRecording(){
        if(recorder.isRecording()){
            recorder.stop();record.setButtonText("RECORD VOCAL");
            // Stop the transport before changing the audio graph.
            for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(0);}
            playing.store(false);stopTimer();sessionFrames.store(0);playheadBeats=0;
            const auto take=recorder.file();
            if(!importTake(take))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Take saved, import failed",take.getFullPathName());
            return;
        }
        const auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile("IDW DAW").getChildFile("Recordings");
        folder.createDirectory();
        auto destination=folder.getNonexistentChildFile("Vocal Take",".wav",false);
        // Anchor the take at the shared timeline location at record start.
        // Hardware input/output latency compensation remains a future feature.
        takeStartBeat=deviceRate>0 ? (double)sessionFrames.load()*session.bpm/(deviceRate*60.0) : 0.0;
        if(recorder.start(destination,deviceRate))record.setButtonText("STOP RECORD");
        else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Recording unavailable","Check microphone permission, device input and destination directory.");
    }
    void loadAudio(){
        chooser=std::make_unique<juce::FileChooser>("Select WAV or AIFF audio file",juce::File{},"*.wav;*.aiff;*.aif");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser& picker){
            auto file=picker.getResult();if(!file.existsAsFile() || playing.load())return;
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
    ~Workstation() override {if(exportThread.joinable())exportThread.join();recorder.stop();deviceManager.removeAudioCallback(&recorder);shutdownAudio();for(auto& slot:sources)slot->transport.setSource(nullptr);sources.clear();}
    std::thread exportThread;
    std::atomic<bool> exportRunning{false};
    std::atomic<bool> exportCancel{false};
    std::atomic<float> exportProgress{0.0f};
    idw::daw::Session session;
    std::atomic<bool> playing{false};
    double playheadBeats=0;
    double takeStartBeat=0.0;
    juce::Label title,tempoLabel,gainLabel,panLabel;
    juce::Slider gainControl,panControl,masterGain;
    juce::Label masterLabel,eqLabel,limitLabel;
    juce::Slider ceilingControl;
    std::atomic<float> ceilingDb{-1.0f};
    juce::Slider lowEq,highEq;
    std::atomic<float> lowDb{0.0f},highDb{0.0f};
    float eqState[2]{0.0f,0.0f};
    juce::TextButton clearClips;
    std::atomic<float> peakLeft{0.0f},peakRight{0.0f};
    std::atomic<bool> peakClipLeft{false},peakClipRight{false};
    std::atomic<float> masterLevel{1.0f};
    juce::ToggleButton muteControl,soloControl;
    juce::TextButton previousTrack,nextTrack;
    size_t selectedTrack=0;
    juce::TextButton play,stop,audio,midi,open,record,saveProject,openProject,exportProject,bounce,cancelBounce,connectButton,aiButton,pianoButton;
    juce::ComboBox synthPreset;
    IDWPianoRoll pianoRoll;
    IDWAIApprovalPanel aiPanel;
    IDWConnectPanel connectPanel;
    IDWTakeRecorder recorder;
    juce::AudioFormatManager formats;
    std::atomic<int64_t> sessionFrames{0};
    double deviceRate=48000.0;
    int preparedBlockSize=0;
    std::vector<std::unique_ptr<AudioSlot>> sources;
    std::unique_ptr<juce::FileChooser> chooser,projectChooser;
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
