#pragma once
#include <JuceHeader.h>
// Signals are observed locally; audible output is confirmed only by the user.
class ConnectionPanel final : public juce::Component {
public:
    ConnectionPanel(std::function<void()> close, std::function<void()> manual,
                    std::function<void()> test, std::function<void(bool)> localSound,
                    std::function<void()> copy) {
        for (auto* c : std::initializer_list<juce::Component*>{&title,&done,&route,&instructions,&signals,&testButton,&localButton,&externalButton,&heard,&copyButton,&manualButton,&hint}) addAndMakeVisible(c);
        title.setText("CONNECTION CHECK / VOICE TO SOUND",juce::dontSendNotification);
        done.setButtonText("Close setup"); done.onClick=std::move(close);
        route.addItemList({"IDW built-in sound", "FL Studio VST3", "Other Windows VST3 host", "Standalone MIDI output"},1);
        route.onChange=[this]{resetConfirmation();refreshInstructions();}; route.setSelectedId(1,juce::dontSendNotification);refreshInstructions();
        for(auto* label:{&instructions,&signals,&hint}) label->setJustificationType(juce::Justification::topLeft);
        signals.setFont(juce::FontOptions(18));
        testButton.setButtonText("Send test note");
        testButton.onClick=[this,test]{if(!running)return;resetConfirmation();testSent=true;heard.setEnabled(true);test();};
        localButton.setButtonText("Enable IDW instrument");localButton.onClick=[this,localSound]{resetConfirmation();localSound(true);};
        externalButton.setButtonText("Mute IDW sounds / use DAW");externalButton.onClick=[this,localSound]{resetConfirmation();localSound(false);};
        heard.setButtonText("I heard this test note (manual confirmation)");heard.setEnabled(false);
        copyButton.setButtonText("Copy setup report");copyButton.onClick=std::move(copy);
        manualButton.setButtonText("Full manual");manualButton.onClick=std::move(manual);
        hint.setText("Start with headphones. Test note bypasses microphone tracking and sends C4 / MIDI 60 on channel 1.\nA generated event is not proof your DAW received it. Confirm only after hearing the selected instrument.\nChanging the route or sending another test clears the confirmation. It is never saved as a compatibility claim.",juce::dontSendNotification);
    }
    void begin(int events){baseline=events;resetConfirmation();}
    void update(bool active,float peak,float rms,int events,const juce::String& guidance,bool internal) {
        if(running && !active)resetConfirmation(); running=active;
        testButton.setEnabled(active);
        const int count=juce::jmax(0,events-baseline);
        signals.setText(juce::String("1. Audio processing: ")+(active?"RUNNING":"STOPPED")
          +"\n2. Microphone input: "+juce::String(juce::Decibels::gainToDecibels(rms,-100.0f),1)+" dBFS"+(peak>=.98f?" / CLIPPING":"")
          +"\n3. Generated MIDI since opening: "+juce::String(count)+" events"
          +"\n4. IDW sound: "+(internal?"enabled":"off / external instrument needed")
          +"\n\n"+guidance,juce::dontSendNotification);
    }
    juce::String report() const {return "Setup route: "+route.getText()+"\nTest requested this check: "+(testSent?"yes":"no")+"\nUser confirmed audible note: "+(heard.getToggleState()?"yes (manual)":"no / unverified")+"\n";}
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff141a24));g.setColour(juce::Colour(0xffeac36c));g.drawRect(getLocalBounds(),2);}
    void resized() override {
        const int w=getWidth();title.setBounds(24,18,w-240,32);done.setBounds(w-190,18,165,32);
        route.setBounds(24,70,w-48,34);instructions.setBounds(24,122,w-48,142);
        signals.setBounds(24,280,w-48,170);
        localButton.setBounds(24,468,225,36);externalButton.setBounds(264,468,260,36);testButton.setBounds(540,468,180,36);
        heard.setBounds(24,522,w-48,34);hint.setBounds(24,580,w-48,90);
        copyButton.setBounds(24,getHeight()-62,200,36);manualButton.setBounds(240,getHeight()-62,170,36);
    }
private:
    void resetConfirmation(){testSent=false;heard.setToggleState(false,juce::dontSendNotification);heard.setEnabled(false);}
    void refreshInstructions(){
        juce::String text;
        switch(route.getSelectedId()){
        case 1:text="Select your audio devices in standalone Options > Audio/MIDI Settings, or in your DAW.\nClick Enable IDW instrument, then Send test note. If silent, check output selection, volume and track monitoring.\nOnce the tone is audible, sing a steady note and watch the input and MIDI readings.";break;
        case 2:text="Load IDW on the mixer insert receiving your microphone. Select the correct input and enable processing.\nMatch IDW wrapper MIDI Output port to the target instrument wrapper Input port. Use channel 1 for the test.\nClick Mute IDW sounds / use DAW, then Send test note. Check the target instrument and its mixer output.\nFor layered voices: lead 1, chords 2, bass 3, drums 10. Start with MPE off.";break;
        case 3:text="Feed microphone audio into IDW's VST3 track. Create a destination instrument track.\nChoose IDW's generated MIDI as that track's input; enable monitoring/record arm as required by your host.\nClick Mute IDW sounds / use DAW, then Send test note. Verify the destination receives MIDI and plays sound.\nHost routing and channel handling vary. Direct loading requires Windows x64 VST3 support.";break;
        default:text="In standalone Options > Audio/MIDI Settings, choose an existing MIDI output port.\nEnable the matching input in your DAW, select it on an instrument track, and enable monitoring.\nA real or virtual MIDI port must already exist; IDW does not install a virtual MIDI driver.\nClick Mute IDW sounds / use DAW, then Send test note. Check both MIDI activity and audible output.";break;
        }
        instructions.setText(text,juce::dontSendNotification);
    }
    juce::Label title,instructions,signals,hint;juce::ComboBox route;
    juce::TextButton done,testButton,localButton,externalButton,copyButton,manualButton;
    juce::ToggleButton heard;bool running=false,testSent=false;int baseline=0;
};
