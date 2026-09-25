#pragma once
#include <JuceHeader.h>
// APVTS retains current parameter values for missing nodes. Legacy V4 states must
// explicitly default V5-only controls, otherwise loading an old preset can leave chords on.
inline juce::ValueTree withV5Defaults(juce::ValueTree tree,juce::AudioProcessorValueTreeState& state){
    for(const char* id:{"harmonyMode","harmonyVoicing","harmonyBass","voiceLow","voiceHigh","synthEnabled","synthLead","synthChord","synthBass","synthGain","synthTone","synthAttack","synthRelease","synthDelay","retroEnabled"}){
        if(!tree.getChildWithProperty("id",id).isValid())if(auto* parameter=state.getParameter(id)){
            juce::ValueTree node("PARAM");node.setProperty("id",id,nullptr);
            node.setProperty("value",parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);tree.addChild(node,-1,nullptr);
        }
    }
    return tree;
}
