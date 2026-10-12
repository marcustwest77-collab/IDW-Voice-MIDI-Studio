#include <JuceHeader.h>
#include "SessionModel.h"
#include "TimelineMath.h"
#include "TakeRecorder.h"
#include <atomic>
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
        for(auto* b:{&play,&stop,&audio,&midi,&open,&record}) addAndMakeVisible(*b);
        play.setButtonText("PLAY"); stop.setButtonText("STOP"); open.setButtonText("OPEN WAV/AIFF");
        record.setButtonText("RECORD VOCAL");
        audio.setButtonText("+ AUDIO TRACK"); midi.setButtonText("+ MIDI TRACK");
        play.onClick=[this]{if(loadedCount()>0){sessionFrames.store(0);for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(slot->sourceOffsetSeconds);slot->transport.start();}playing=true;startTimerHz(30);repaint();}};
        stop.onClick=[this]{if(recorder.isRecording())toggleRecording();for(auto& slot:sources){slot->transport.stop();slot->transport.setPosition(slot->sourceOffsetSeconds);}sessionFrames.store(0);playing=false;stopTimer();playheadBeats=0;repaint();};
        open.onClick=[this]{loadAudio();};
        record.onClick=[this]{toggleRecording();};
        audio.onClick=[this]{addAudioTrack();repaint();};
        midi.onClick=[this]{if(!playing.load())session.addTrack(idw::daw::TrackType::midi);repaint();};
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
        muteControl.setButtonText("MUTE TRACK 1");addAndMakeVisible(muteControl);
        gainControl.onValueChange=[this]{updateFirstTrackMix();};
        panControl.onValueChange=[this]{updateFirstTrackMix();};
        muteControl.onClick=[this]{
            if(playing.load() || session.tracks.empty())return;
            session.tracks[0].mute=muteControl.getToggleState();repaint();
        };
    }
    void resized() override {
        auto area=getLocalBounds().reduced(20);
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
        auto mixRow=area.removeFromTop(42);
        gainLabel.setBounds(mixRow.removeFromLeft(110));
        gainControl.setBounds(mixRow.removeFromLeft(185));
        panLabel.setBounds(mixRow.removeFromLeft(45));
        panControl.setBounds(mixRow.removeFromLeft(185));
        muteControl.setBounds(mixRow.removeFromLeft(160));
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff101318));
        const auto rect=getLocalBounds().reduced(20);
        const int top=170, trackHeight=70, left=205;
        g.setColour(juce::Colour(0xff1b222b));g.fillRect(20,top,getWidth()-40,36);
        g.setColour(juce::Colours::lightgrey);
        for(int bar=0;bar<17;++bar){
            const int x=left+bar*48;
            if(x>=getWidth()-30)break;
            g.drawVerticalLine(x,(float)top,(float)(getHeight()-30));
            g.drawText(juce::String(bar+1),x+3,top,40,30,juce::Justification::centredLeft);
        }
        int y=top+36;
        for(const auto& track:session.tracks){
            g.setColour(juce::Colour((y/trackHeight)%2 ? 0xff252b35 : 0xff1c222a));
            g.fillRect(20,y,getWidth()-40,trackHeight-2);
            g.setColour(track.type==idw::daw::TrackType::audio ? juce::Colours::orange : juce::Colours::deepskyblue);
            g.fillRect(20,y,5,trackHeight-2);
            g.setColour(juce::Colours::white);
            g.drawText(juce::String(track.name),35,y+10,155,35,juce::Justification::centredLeft);
            for(const auto& clip:track.clips){
                const int clipX=left+juce::roundToInt(clip.startBeat*12.0);
                const int clipWidth=juce::jmax(2,juce::roundToInt(clip.lengthBeats*12.0));
                if(clipX>=getWidth()-25)continue;
                g.setColour(juce::Colour(0xff89551e));
                g.fillRoundedRectangle((float)clipX,(float)y+8,(float)juce::jmin(clipWidth,getWidth()-25-clipX), (float)trackHeight-18,4.0f);
                g.setColour(juce::Colours::white);
                g.drawFittedText(juce::File(juce::String(clip.sourcePath)).getFileName(),clipX+5,y+14,juce::jmax(0,juce::jmin(clipWidth-10,getWidth()-35-clipX)),25,juce::Justification::centredLeft,1);
            }
            y+=trackHeight;
            if(y>getHeight()-60)break;
        }
        const int head=left+(int)(playheadBeats*12.0);
        if(head<getWidth()-30){
            g.setColour(juce::Colours::gold);
            g.drawVerticalLine(head,(float)top,(float)(getHeight()-30));
        }
        g.setColour(juce::Colours::grey);
        g.drawFittedText(!sources.empty() ? "Loaded audio tracks: " + juce::String(loadedCount()) + "  |  Timeline-scheduled playback prototype; recording and clip editing unavailable." : "Add an audio track and open WAV/AIFF files to mix their playback.",25,getHeight()-44,getWidth()-50,35,juce::Justification::centredLeft,1);
    }
private:
    void prepareToPlay(int samplesPerBlockExpected,double sampleRate) override {
        deviceRate=sampleRate;
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
        for(auto& slot:sources) {
            if(!slot->reader || slot->trackIndex>=session.tracks.size())continue;
            const auto& track=session.tracks[slot->trackIndex];
            if(track.mute || track.clips.empty())continue;
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
        sessionFrames.fetch_add(info.numSamples,std::memory_order_relaxed);
    }
    void releaseResources() override {
        for(auto& slot:sources)slot->transport.releaseResources();
    }
    void updateFirstTrackMix(){
        if(playing.load() || session.tracks.empty())return;
        session.setTrackMix(0,(float)gainControl.getValue(),(float)panControl.getValue());
    }
    void timerCallback() override {
        playheadBeats=deviceRate>0?(double)sessionFrames.load()*session.bpm/(deviceRate*60.0):0.0;
        if(playing && playheadBeats>=lastClipEndBeat()){playing=false;stopTimer();for(auto& slot:sources)slot->transport.stop();}
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
    double lastClipEndBeat() const {double end=0;for(const auto& track:session.tracks)for(const auto& clip:track.clips)end=juce::jmax(end,clip.startBeat+clip.lengthBeats);return end;}
    int loadedCount() const {
        int count=0;for(auto& slot:sources)if(slot->reader)++count;
        return count;
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
    ~Workstation() override {recorder.stop();deviceManager.removeAudioCallback(&recorder);shutdownAudio();for(auto& slot:sources)slot->transport.setSource(nullptr);sources.clear();}
    idw::daw::Session session;
    std::atomic<bool> playing{false};
    double playheadBeats=0;
    double takeStartBeat=0.0;
    juce::Label title,tempoLabel,gainLabel,panLabel;
    juce::Slider gainControl,panControl;
    juce::ToggleButton muteControl;
    juce::TextButton play,stop,audio,midi,open,record;
    IDWTakeRecorder recorder;
    juce::AudioFormatManager formats;
    std::atomic<int64_t> sessionFrames{0};
    double deviceRate=48000.0;
    int preparedBlockSize=0;
    std::vector<std::unique_ptr<AudioSlot>> sources;
    std::unique_ptr<juce::FileChooser> chooser;
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
