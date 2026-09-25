#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
// A voice profile changes input calibration only, never a musical preset or drum model.
class VoiceProfiles {
public:
    static juce::File directory(){auto d=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IDW Voice MIDI Studio/Voice Profiles");d.createDirectory();return d;}
    static juce::StringArray names(){juce::StringArray result;for(auto f:directory().findChildFiles(juce::File::findFiles,false,"*.xml"))result.add(f.getFileNameWithoutExtension());result.sort(true);return result;}
    static juce::String safeName(const juce::String& s){return juce::File::createLegalFileName(s.trim()).substring(0,80);}
    static bool save(juce::AudioProcessorValueTreeState& state,const juce::String& name){
        const auto clean=safeName(name);if(clean.isEmpty())return false;
        juce::XmlElement xml("IDWVoiceProfile");xml.setAttribute("version",1);
        for(const char* id:ids())xml.setAttribute(id,(double)state.getRawParameterValue(id)->load());
        juce::TemporaryFile file(directory().getChildFile(clean+".xml"));
        return xml.writeTo(file.getFile()) && file.overwriteTargetFileWithTemporary();
    }
    static bool load(juce::AudioProcessorValueTreeState& state,const juce::String& name){
        auto xml=juce::XmlDocument::parse(directory().getChildFile(safeName(name)+".xml"));
        if(!xml||!xml->hasTagName("IDWVoiceProfile")||xml->getIntAttribute("version")!=1)return false;
        for(const char* id:ids())if(!xml->hasAttribute(id)||!std::isfinite(xml->getDoubleAttribute(id)))return false;
        for(const char* id:ids())if(auto* parameter=state.getParameter(id)){
            parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1((float)xml->getDoubleAttribute(id)));parameter->endChangeGesture();
        }
        return true;
    }
private:
    static std::array<const char*,6> ids(){return {{"gate","confidence","inputMode","tuneCents","voiceLow","voiceHigh"}};}
};
