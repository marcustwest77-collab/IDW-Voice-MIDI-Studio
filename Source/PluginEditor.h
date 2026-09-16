#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class IDWVoiceMIDIStudioAudioProcessorEditor:public juce::AudioProcessorEditor,private juce::Timer{
public:explicit IDWVoiceMIDIStudioAudioProcessorEditor(IDWVoiceMIDIStudioAudioProcessor&);void paint(juce::Graphics&)override;void resized()override;
private:void timerCallback()override;void syncScale();IDWVoiceMIDIStudioAudioProcessor&p;juce::Label title,readout,status;juce::TextButton scaleButtons[12],learn{"MIDI LEARN"},save{"SAVE PRESET"};juce::ComboBox learnTarget;
JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IDWVoiceMIDIStudioAudioProcessorEditor)};
