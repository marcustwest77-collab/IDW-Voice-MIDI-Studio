#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <cstdlib>
class SongScenes {
public:
    static juce::File directory(){auto d=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("IDW Voice MIDI Studio/Song Scenes");d.createDirectory();return d;}
    static juce::String safeName(const juce::String& name){return juce::File::createLegalFileName(name.trim()).substring(0,80);}
    static juce::StringArray names(){juce::StringArray result;for(auto f:directory().findChildFiles(juce::File::findFiles,false,"*.xml"))result.add(f.getFileNameWithoutExtension());result.sort(true);return result;}
    static bool save(juce::AudioProcessorValueTreeState& state,const juce::String& name){
        auto clean=safeName(name);if(clean.isEmpty())return false;juce::XmlElement xml("IDWSongScene");xml.setAttribute("version",1);
        for(const char* id:ids())xml.setAttribute(id,(double)state.getRawParameterValue(id)->load());
        juce::TemporaryFile temp(directory().getChildFile(clean+".xml"));return xml.writeTo(temp.getFile())&&temp.overwriteTargetFileWithTemporary();
    }
    static bool load(juce::AudioProcessorValueTreeState& state,const juce::String& name){
        const auto file=directory().getChildFile(safeName(name)+".xml");if(file.getSize()>65536)return false;
        const auto xml=juce::XmlDocument::parse(file);if(!xml||!xml->hasTagName("IDWSongScene")||xml->getIntAttribute("version")!=1)return false;
        std::vector<std::pair<juce::RangedAudioParameter*,float>>values;
        for(const char* id:ids()){
            auto* parameter=state.getParameter(id);if(!parameter||!xml->hasAttribute(id))return false;
            const auto text=xml->getStringAttribute(id).trim().toStdString();char* end=nullptr;const double value=std::strtod(text.c_str(),&end);
            if(text.empty()||end!=text.c_str()+text.size()||!std::isfinite(value))return false;
            const auto range=parameter->getNormalisableRange();if(value<range.start||value>range.end)return false;
            values.emplace_back(parameter,(float)value);
        }
        // Validate the entire file before touching any parameter.
        for(auto pair:values){pair.first->beginChangeGesture();pair.first->setValueNotifyingHost(pair.first->convertTo0to1(pair.second));pair.first->endChangeGesture();}
        return true;
    }
private:
    static std::initializer_list<const char*> ids(){
        // Static storage avoids returning a dangling initializer_list backing array.
        static const std::initializer_list<const char*> list={"synthEnabled","synthLead","synthChord","synthBass","synthGain","synthTone","synthAttack","synthRelease","synthDelay","harmonyMode","harmonyVoicing","harmonyBass","root","scaleLock","scaleMask","scaleExpression","bend","beatbox","kick","snare","hat","pad4","pad5","pad6","pad7","pad8"};return list;
    }
};
