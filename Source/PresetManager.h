#pragma once
#include <JuceHeader.h>
class PresetManager{public:explicit PresetManager(juce::AudioProcessorValueTreeState&s):state(s){}bool save(const juce::String&);bool load(const juce::String&);juce::StringArray list()const;
private:juce::AudioProcessorValueTreeState&state;juce::File dir()const;};
