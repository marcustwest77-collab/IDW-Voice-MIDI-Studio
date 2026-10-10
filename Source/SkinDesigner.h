#pragma once
#include <JuceHeader.h>
#include "CustomSkin.h"

class SkinDesigner final : public juce::Component {
public:
    SkinDesigner(juce::AudioProcessorValueTreeState& state,std::function<void()> close):apvts(state){
        title.setText("CUSTOM SKIN DESIGNER",juce::dontSendNotification);
        addAndMakeVisible(title);
        for(size_t i=0;i<fields.size();++i){
            labels[i].setText(idw::skinColourNames[i],juce::dontSendNotification);
            fields[i].setTooltip("Six hexadecimal RGB digits, for example #EAC36C. Press Apply colours to preview.");
            addAndMakeVisible(labels[i]);addAndMakeVisible(fields[i]);
        }
        for(auto* c:std::initializer_list<juce::Component*>{&name,&saved,&apply,&save,&load,&reset,&done,&message})addAndMakeVisible(c);
        name.setTextToShowWhenEmpty("Name your skin",juce::Colours::grey);
        name.setInputRestrictions(64);
        apply.onClick=[this]{applyFields();};
        save.onClick=[this]{saveSkin();};
        load.onClick=[this]{loadSkin();};
        reset.onClick=[this]{set("customSkinEnabled",0);message.setText("Built-in skin restored. Your custom colours are kept.",juce::dontSendNotification);};
        // Keep the recovery action legible even when custom text/background clash.
        reset.setColour(juce::TextButton::buttonColourId,juce::Colours::black);
        reset.setColour(juce::TextButton::textColourOffId,juce::Colours::white);
        done.onClick=std::move(close);
        refresh();
    }
    void refresh(){
        for(size_t i=0;i<fields.size();++i)fields[i].setText(juce::String::toHexString((int)apvts.getRawParameterValue(idw::skinColourIds[i])->load()).paddedLeft('0',6),false);
        saved.clear(juce::dontSendNotification);files.clear();
        directory().findChildFiles(files,juce::File::findFiles,false,"*.xml");
        for(int i=0;i<files.size();++i)if(auto xml=juce::XmlDocument::parse(files[i]))
            if(xml->hasTagName("IDWSkin"))saved.addItem(xml->getStringAttribute("name",files[i].getFileNameWithoutExtension()),i+1);
        saved.setTextWhenNothingSelected("Choose a saved skin");
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(findColour(juce::ComboBox::backgroundColourId));
        g.setColour(findColour(juce::ComboBox::outlineColourId));g.drawRect(getLocalBounds(),2);
    }
    void resized() override {
        title.setBounds(24,16,getWidth()-200,32);done.setBounds(getWidth()-140,16,116,32);
        for(size_t i=0;i<fields.size();++i){int y=66+(int)i*48;labels[i].setBounds(24,y,140,32);fields[i].setBounds(180,y,200,32);}
        apply.setBounds(24,314,160,34);reset.setBounds(196,314,200,34);
        name.setBounds(24,366,260,32);save.setBounds(296,366,120,32);
        saved.setBounds(24,414,260,32);load.setBounds(296,414,120,32);
        message.setBounds(24,464,getWidth()-48,72);
    }
private:
    static juce::File directory(){return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IDW Voice MIDI Studio/Skins");}
    void set(const char* id,float value){auto* p=apvts.getParameter(id);p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(value));p->endChangeGesture();}
    bool read(std::array<std::uint32_t,5>& values){
        for(size_t i=0;i<fields.size();++i)if(!idw::parseSkinHex(fields[i].getText().trim().toStdString(),values[i])){
            message.setText(juce::String(idw::skinColourNames[i])+": enter six hex digits, e.g. #EAC36C. Nothing was applied.",juce::dontSendNotification);return false;
        }
        return true;
    }
    bool applyFields(){
        std::array<std::uint32_t,5> values{};if(!read(values))return false;
        for(size_t i=0;i<values.size();++i)set(idw::skinColourIds[i],(float)values[i]);
        set("customSkinEnabled",1);message.setText("Custom colours applied. Use Built-in skin if text becomes hard to read.",juce::dontSendNotification);return true;
    }
    void saveSkin(){
        const auto label=name.getText().trim();if(label.isEmpty()){message.setText("Enter a name before saving.",juce::dontSendNotification);return;}
        std::array<std::uint32_t,5> values{};if(!read(values))return;
        juce::XmlElement xml("IDWSkin");xml.setAttribute("version",1);xml.setAttribute("name",label);
        xml.setAttribute("base",(int)apvts.getRawParameterValue("uiSkin")->load());
        for(size_t i=0;i<values.size();++i)xml.setAttribute(idw::skinColourIds[i],juce::String::toHexString((int)values[i]).paddedLeft('0',6));
        if(directory().createDirectory().failed()){message.setText("Could not create the skin folder.",juce::dontSendNotification);return;}
        const auto file=directory().getChildFile(juce::Uuid().toString()+".xml");juce::TemporaryFile temp(file);
        if(!temp.getFile().replaceWithText(xml.toString())||!temp.overwriteTargetFileWithTemporary()){
            message.setText("Could not save the skin. Existing skins are preserved.",juce::dontSendNotification);return;
        }
        applyFields();refresh();message.setText("Saved "+label+". It is available to other IDW projects on this computer.",juce::dontSendNotification);
    }
    void loadSkin(){
        const int index=saved.getSelectedId()-1;if(!juce::isPositiveAndBelow(index,files.size()))return;
        auto xml=juce::XmlDocument::parse(files[index]);std::array<std::uint32_t,5> values{};
        bool valid=xml && xml->hasTagName("IDWSkin") && xml->getIntAttribute("version")==1;
        if(valid)for(size_t i=0;i<values.size();++i)valid=idw::parseSkinHex(xml->getStringAttribute(idw::skinColourIds[i]).toStdString(),values[i])&&valid;
        const int base=xml?xml->getIntAttribute("base",-1):-1;
        if(!valid||base<0||base>=idw::skinCount()){message.setText("Invalid or unsupported skin file. Current colours kept.",juce::dontSendNotification);return;}
        set("uiSkin",(float)base);
        for(size_t i=0;i<values.size();++i)fields[i].setText(juce::String::toHexString((int)values[i]).paddedLeft('0',6),false);
        name.setText(xml->getStringAttribute("name"),false);applyFields();
    }
    juce::AudioProcessorValueTreeState& apvts;
    juce::Label title,message;std::array<juce::Label,5> labels;
    std::array<juce::TextEditor,5> fields;juce::TextEditor name;juce::ComboBox saved;
    juce::TextButton apply{"Apply colours"},save{"Save new skin"},load{"Load skin"},reset{"Built-in skin"},done{"Done"};
    juce::Array<juce::File> files;
};
