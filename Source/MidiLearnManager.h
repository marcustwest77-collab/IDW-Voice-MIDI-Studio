#pragma once
#include <JuceHeader.h>
#include <array>
class MidiLearnManager {
public:
    MidiLearnManager();
    void prepare(juce::AudioProcessorValueTreeState&);
    void arm(const juce::String&);
    void clear();
    bool process(const juce::MidiBuffer&,juce::AudioProcessorValueTreeState&);
    juce::String mappingText() const;
    juce::StringArray destinations() const;
    juce::ValueTree save() const;
    void restore(const juce::ValueTree&);
private:
    static constexpr int destinationCount=6;
    std::array<std::atomic<int>,2048> mappings; // channel-aware, no map allocation in audio callback
    std::array<juce::RangedAudioParameter*,destinationCount> parameters{};
    std::atomic<int> armed{-1},lastMapping{-1};
};
