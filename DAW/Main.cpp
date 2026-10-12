#include <JuceHeader.h>
#include "SessionModel.h"
class Workstation final : public juce::Component, private juce::Timer {
public:
    Workstation() {
        setSize(1100,700);
        title.setText("IDW  |  VOICE MIDI STUDIO DAW  |  V14 FOUNDATION",juce::dontSendNotification);
        title.setColour(juce::Label::textColourId,juce::Colours::gold);
        title.setFont(juce::Font(juce::FontOptions(22.0f,juce::Font::bold)));
        addAndMakeVisible(title);
        for(auto* b:{&play,&stop,&audio,&midi}) addAndMakeVisible(*b);
        play.setButtonText("PLAY"); stop.setButtonText("STOP");
        audio.setButtonText("+ AUDIO TRACK"); midi.setButtonText("+ MIDI TRACK");
        play.onClick=[this]{playing=true;startTimerHz(30);repaint();};
        stop.onClick=[this]{playing=false;stopTimer();playheadBeats=0;repaint();};
        audio.onClick=[this]{session.addTrack(idw::daw::TrackType::audio);repaint();};
        midi.onClick=[this]{session.addTrack(idw::daw::TrackType::midi);repaint();};
        tempo.setRange(20,300,1);tempo.setValue(120);tempo.setTextBoxStyle(juce::Slider::TextBoxRight,false,70,28);
        tempo.onValueChange=[this]{session.setTempo(tempo.getValue());};addAndMakeVisible(tempo);
        tempoLabel.setText("BPM",juce::dontSendNotification);addAndMakeVisible(tempoLabel);
        session.addTrack(idw::daw::TrackType::audio);
        session.addTrack(idw::daw::TrackType::midi);
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
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff101318));
        const auto rect=getLocalBounds().reduced(20);
        const int top=120, trackHeight=70, left=205;
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
            y+=trackHeight;
            if(y>getHeight()-60)break;
        }
        const int head=left+(int)(playheadBeats*12.0);
        if(head<getWidth()-30){
            g.setColour(juce::Colours::gold);
            g.drawVerticalLine(head,(float)top,(float)(getHeight()-30));
        }
        g.setColour(juce::Colours::grey);
        g.drawFittedText("Foundation prototype: transport preview only. No audio recording, playback or clip editing yet.",25,getHeight()-44,getWidth()-50,35,juce::Justification::centredLeft,1);
    }
private:
    void timerCallback() override {playheadBeats+=session.bpm/(60.0*30.0);repaint();}
    idw::daw::Session session;
    bool playing=false;
    double playheadBeats=0;
    juce::Label title,tempoLabel;
    juce::TextButton play,stop,audio,midi;
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
