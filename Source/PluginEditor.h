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
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void syncScale();
    void refreshPresets();
    void styleSlider(juce::Slider&, const juce::String& suffix = {});
    void styleButton(juce::TextButton&);
    void styleCaption(juce::Label&, const juce::String&);

    IDWVoiceMIDIStudioAudioProcessor& p;

    juce::Label title, subtitle, readout, status;
    juce::Label gateCaption, confidenceCaption, bendCaption, beatCaption, latencyCaption;
    juce::Label rootCaption, presetCaption, learnCaption;

    juce::Slider gate, confidenceSlider, bendSlider, beatThreshold, latencySlider;
    juce::TextButton scaleButtons[12];
    juce::TextButton scaleLock { "SCALE LOCK" };
    juce::TextButton beatbox { "BEATBOX" };
    juce::TextButton gesture { "GESTURE CC" };
    juce::TextButton mpe { "MPE" };
    juce::TextButton calibrate { "CALIBRATE MIC" };
    juce::TextButton learn { "MIDI LEARN" };
    juce::TextButton save { "SAVE PRESET" };
    juce::TextButton load { "LOAD PRESET" };

    juce::ComboBox root, learnTarget, presetList;

    std::unique_ptr<SliderAttachment> gateAttachment, confidenceAttachment, bendAttachment,
                                      beatAttachment, latencyAttachment;
    std::unique_ptr<ButtonAttachment> scaleLockAttachment, beatboxAttachment,
                                      gestureAttachment, mpeAttachment;
    std::unique_ptr<ComboBoxAttachment> rootAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IDWVoiceMIDIStudioAudioProcessorEditor)
};
