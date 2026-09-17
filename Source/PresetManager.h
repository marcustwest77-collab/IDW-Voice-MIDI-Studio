#pragma once
#include <JuceHeader.h>

class PresetManager
{
public:
    explicit PresetManager(juce::AudioProcessorValueTreeState& s) : state(s) {}

    bool save(const juce::String&);
    bool load(const juce::String&);
    juce::StringArray list() const;

    juce::StringArray factoryPresetNames() const;
    bool applyFactoryPreset(const juce::String& name);

private:
    juce::AudioProcessorValueTreeState& state;
    juce::File dir() const;
    void setParameter(const juce::String& id, float value);
};
