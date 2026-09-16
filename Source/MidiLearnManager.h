#pragma once
#include <JuceHeader.h>
class MidiLearnManager{public:void arm(const juce::String&id){target=id;armed=true;}bool process(const juce::MidiBuffer&,juce::AudioProcessorValueTreeState&);
juce::String mappingText()const{return last;}juce::StringArray destinations()const{return {"gate","confidence","bend","beatThreshold","ccSense","latencyMs"};}
private:bool armed=false;juce::String target,last;std::map<int,juce::String>map;};
