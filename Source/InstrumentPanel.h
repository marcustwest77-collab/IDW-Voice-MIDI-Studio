#pragma once
#include <JuceHeader.h>
#include "SongScenes.h"
class InstrumentPanel : public juce::Component {
public:
    explicit InstrumentPanel(juce::AudioProcessorValueTreeState& state,std::function<void()> close,std::function<void()> sceneChanged={}):onClose(std::move(close)){
        addAndMakeVisible(sceneSelector);sceneSelector.setEditableText(true);sceneSelector.setTextWhenNothingSelected("Scene name: Verse, Hook, Bridge");
        refreshScenes();addAndMakeVisible(saveScene);saveScene.setButtonText("Save scene");addAndMakeVisible(sceneStatus);
        sceneStatus.setText("Scenes recall sounds, harmony, scale and drum-note settings. Microphone calibration, voice range, tempo and MIDI routing stay as set.",juce::dontSendNotification);
        sceneSelector.onChange=[this,&state,sceneChanged]{const auto index=sceneSelector.getSelectedId()-1;if(juce::isPositiveAndBelow(index,sceneNames.size())){const bool ok=SongScenes::load(state,sceneNames[index]);if(ok&&sceneChanged)sceneChanged();sceneStatus.setText(ok?"Scene loaded. Held notes released; sing or play the next phrase.":"Could not load this scene. Current settings kept.",juce::dontSendNotification);}};
        saveScene.onClick=[this,&state]{const auto name=SongScenes::safeName(sceneSelector.getText());const bool ok=SongScenes::save(state,name);if(ok){sceneSelector.setText(name,juce::dontSendNotification);refreshScenes();}sceneStatus.setText(ok?"Scene saved. Saving the same name replaces that scene.":"Enter a scene name and check folder permissions.",juce::dontSendNotification);};
        addAndMakeVisible(title);title.setText("IDW STUDIO INSTRUMENT",juce::dontSendNotification);
        addAndMakeVisible(enabled);enabled.setButtonText("Enable built-in instrument");
        enableAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(state,"synthEnabled",enabled);
        addAndMakeVisible(done);done.setButtonText("Close instrument");done.onClick=[this]{onClose();};
        const char* names[]={"Lead","Chords","Bass"};const char* ids[]={"synthLead","synthChord","synthBass"};
        for(int i=0;i<3;++i){addAndMakeVisible(patchLabels[i]);patchLabels[i].setText(names[i],juce::dontSendNotification);addAndMakeVisible(patches[i]);patches[i].addItemList({"Sub / sine","Saw lead","Pulse","Warm keys","FM bell"},1);comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(state,ids[i],patches[i]));}
        const char* sliderNames[]={"Volume","Brightness","Attack (seconds)","Release (seconds)","Echo"};
        const char* sliderIDs[]={"synthGain","synthTone","synthAttack","synthRelease","synthDelay"};
        for(int i=0;i<5;++i){addAndMakeVisible(labels[i]);labels[i].setText(sliderNames[i],juce::dontSendNotification);addAndMakeVisible(sliders[i]);sliders[i].setSliderStyle(juce::Slider::LinearHorizontal);sliders[i].setTextBoxStyle(juce::Slider::TextBoxRight,false,80,28);sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state,sliderIDs[i],sliders[i]));}
        addAndMakeVisible(info);info.setText("32 voices | Lead 1 / Chords 2 / Bass 3 / Drums 10\nMPE uses the lead patch on every member channel.\nKick, snare and hi-hat synthesis is included. Echo: 250 ms.\nEnabled instrument replaces the simple preview. Turn it off for external synths.\nIncoming MIDI can play this instrument. Disable Melody for keyboard-only playing.",juce::dontSendNotification);
        info.setJustificationType(juce::Justification::topLeft);
    }
    void paint(juce::Graphics& g) override {g.fillAll(juce::Colour(0xff141a24));g.setColour(juce::Colour(0xffeac36c));g.drawRect(getLocalBounds(),2);}
    void resized() override {
        title.setBounds(24,18,getWidth()-240,32);done.setBounds(getWidth()-200,18,175,32);enabled.setBounds(24,65,310,32);
        sceneSelector.setBounds(350,65,getWidth()-520,32);saveScene.setBounds(getWidth()-155,65,130,32);sceneStatus.setBounds(24,665,getWidth()-48,60);
        const int col=(getWidth()-48)/3;
        for(int i=0;i<3;++i){patchLabels[i].setBounds(24+i*col,114,col-16,24);patches[i].setBounds(24+i*col,145,col-16,32);}
        for(int i=0;i<5;++i){labels[i].setBounds(24,207+i*55,190,28);sliders[i].setBounds(220,203+i*55,getWidth()-250,36);}
        info.setBounds(24,500,getWidth()-48,150);
    }
private:
    void refreshScenes(){const auto selected=sceneSelector.getText();sceneSelector.clear(juce::dontSendNotification);sceneNames=SongScenes::names();for(int i=0;i<sceneNames.size();++i)sceneSelector.addItem(sceneNames[i],i+1);sceneSelector.setText(selected,juce::dontSendNotification);}
    juce::ComboBox sceneSelector;juce::TextButton saveScene;juce::Label sceneStatus;juce::StringArray sceneNames;
    std::function<void()>onClose;juce::Label title,info;juce::ToggleButton enabled;juce::TextButton done;
    std::array<juce::ComboBox,3>patches;std::array<juce::Label,3>patchLabels;
    std::array<juce::Slider,5>sliders;std::array<juce::Label,5>labels;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>enableAttachment;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>>comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>sliderAttachments;
};
