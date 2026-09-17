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
    void configureSlider(juce::Slider&, juce::Label&, const juce::String&, const juce::String& tooltip);
    void refreshPresets();
    void loadSelectedPreset();
    void showHelp(bool shouldShow);
    static juce::String manualText();

    IDWVoiceMIDIStudioAudioProcessor& p;

    juce::Label title, readout, status, levelReadout;
    juce::Label gateLabel, confidenceLabel, bendLabel, tuneLabel, rootLabel, presetLabel;
    juce::Slider gateSlider, confidenceSlider, bendSlider, tuneSlider;
    juce::ToggleButton scaleLock { "SCALE LOCK" };
    juce::ComboBox rootSelector;
    juce::TextButton calibrate { "CALIBRATE NOISE" };
    juce::TextButton scaleButtons[12];
    juce::TextButton learn { "MIDI LEARN" }, save { "SAVE PRESET" };
    juce::ComboBox learnTarget;

    juce::ComboBox presetSelector;
    juce::TextButton loadPreset { "LOAD PRESET" };
    juce::TextButton help { "HELP / QUICK START" };
    juce::TextButton closeHelp { "CLOSE HELP" };
    juce::TextEditor helpText;
    juce::StringArray userPresetNames;
    int factoryPresetCount = 0;

    juce::TooltipWindow tooltipWindow { this, 650 };

    std::unique_ptr<SliderAttachment> gateAttachment, confidenceAttachment, bendAttachment, tuneAttachment;
    std::unique_ptr<ButtonAttachment> scaleLockAttachment;
    std::unique_ptr<ComboAttachment> rootAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IDWVoiceMIDIStudioAudioProcessorEditor)
};
