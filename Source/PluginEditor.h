#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class IDWVoiceMIDIStudioAudioProcessorEditor : public juce::AudioProcessorEditor,
                                                private juce::Timer
{
public:
    explicit IDWVoiceMIDIStudioAudioProcessorEditor(IDWVoiceMIDIStudioAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void syncScale();
    void calibrateNoiseGate();
    void configureSlider(juce::Slider&, juce::Label&, const juce::String&);

    IDWVoiceMIDIStudioAudioProcessor& p;

    juce::Label title, readout, status, levelReadout;
    juce::Label gateLabel, confidenceLabel, bendLabel, tuneLabel, rootLabel;
    juce::Slider gateSlider, confidenceSlider, bendSlider, tuneSlider;
    juce::ToggleButton scaleLock { "SCALE LOCK" };
    juce::ComboBox rootSelector;
    juce::TextButton calibrate { "CALIBRATE NOISE" };
    juce::TextButton scaleButtons[12];
    juce::TextButton learn { "MIDI LEARN" }, save { "SAVE PRESET" };
    juce::ComboBox learnTarget;

    std::unique_ptr<SliderAttachment> gateAttachment, confidenceAttachment, bendAttachment, tuneAttachment;
    std::unique_ptr<ButtonAttachment> scaleLockAttachment;
    std::unique_ptr<ComboAttachment> rootAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IDWVoiceMIDIStudioAudioProcessorEditor)
};
