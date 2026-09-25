#include "MidiLearnManager.h"
namespace { const char* ids[]={"gate","confidence","bend","beatThreshold","ccSense","scaleExpression"}; }
MidiLearnManager::MidiLearnManager(){clear();}
juce::StringArray MidiLearnManager::destinations() const {juce::StringArray a;for(auto* id:ids)a.add(id);return a;}
void MidiLearnManager::prepare(juce::AudioProcessorValueTreeState& s){for(int i=0;i<destinationCount;++i)parameters[(size_t)i]=s.getParameter(ids[i]);}
void MidiLearnManager::arm(const juce::String& id){armed.store(destinations().indexOf(id));}
void MidiLearnManager::clear(){for(auto& m:mappings)m.store(-1);armed.store(-1);lastMapping.store(-1);}
bool MidiLearnManager::process(const juce::MidiBuffer& buffer,juce::AudioProcessorValueTreeState&) {
    bool handled=false;
    for(const auto metadata:buffer){
        const auto message=metadata.getMessage();if(!message.isController())continue;
        const int slot=(message.getChannel()-1)*128+message.getControllerNumber();
        const int destination=armed.exchange(-1);
        if(destination>=0){mappings[(size_t)slot].store(destination);lastMapping.store(slot);}
        const int mapped=mappings[(size_t)slot].load();
        if(juce::isPositiveAndBelow(mapped,destinationCount)){
            if(auto* parameter=parameters[(size_t)mapped])parameter->setValueNotifyingHost(message.getControllerValue()/127.0f);
            handled=true;
        }
    }
    return handled;
}
juce::String MidiLearnManager::mappingText() const {
    if(armed.load()>=0)return "MIDI Learn armed: move a controller";
    const int slot=lastMapping.load();if(slot<0)return {};
    const int dest=mappings[(size_t)slot].load();if(!juce::isPositiveAndBelow(dest,destinationCount))return {};
    return "CH "+juce::String(slot/128+1)+" CC "+juce::String(slot%128)+" -> "+ids[dest];
}
juce::ValueTree MidiLearnManager::save() const {
    juce::ValueTree tree("MidiMappings");
    for(int i=0;i<2048;++i){const int d=mappings[(size_t)i].load();if(!juce::isPositiveAndBelow(d,destinationCount))continue;
        juce::ValueTree item("Mapping");item.setProperty("slot",i,nullptr);item.setProperty("parameter",ids[d],nullptr);tree.addChild(item,-1,nullptr);}
    return tree;
}
void MidiLearnManager::restore(const juce::ValueTree& tree){
    clear();for(const auto child:tree){const int slot=child.getProperty("slot",-1);const int d=destinations().indexOf(child.getProperty("parameter").toString());
        if(juce::isPositiveAndBelow(slot,2048)&&d>=0)mappings[(size_t)slot].store(d);}
}
