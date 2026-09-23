#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class IDWVoiceMIDIStudioAudioProcessorEditor : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit IDWVoiceMIDIStudioAudioProcessorEditor(IDWVoiceMIDIStudioAudioProcessor&);
    ~IDWVoiceMIDIStudioAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    void timerCallback() override;
    juce::String diagnosticReport() const;
    bool audioRunning() const;
    void configureSlider(juce::Slider&,juce::Label&,const juce::String&,const juce::String&);
    void setupV5();void layoutV5();void refreshProfiles();void recoverTake();
    void setValue(const char*,float);
    void refreshPresets();void loadPreset();void syncScale();void showHelp(bool);
    void beginCapture();void drainCapture();void exportCapture();
    bool writeMidi(const juce::File&);
    static juce::String manualText();
    IDWVoiceMIDIStudioAudioProcessor& p;
    juce::LookAndFeel_V4 theme;
    juce::Label title,readout,status,diagnostics,traceLabel,drumLabel,captureLabel,setupStatus;
    juce::TextButton copyDiagnostics{"Copy diagnostics"};
    unsigned int lastCallbacks=0;
    double lastAudioChange=0;
    float calibrationPeak=0;
    juce::Slider gateSlider,confidenceSlider,bendSlider,tuneSlider,beatSlider,bpmSlider;
    juce::Label gateLabel,confidenceLabel,bendLabel,tuneLabel,beatLabel,bpmLabel;
    juce::ComboBox rootSelector,expressionSelector,inputSelector,presetSelector,learnTarget,trainTarget;
    juce::ToggleButton scaleLock{"Scale lock"},beatbox{"Beatbox"},melody{"Melody"},preview{"Preview sound"},monitor{"Hear microphone"},mpeToggle{"MPE"};
    juce::TextButton scaleButtons[12];
    juce::TextButton calibrate{"Calibrate noise"},panic{"PANIC"},test{"Test note"},help{"Setup / Help"},closeHelp{"Close"};
    juce::TextButton save{"Save preset"},learn{"Learn CC"},clearLearn{"Clear CC"};
    juce::TextButton record{"Record MIDI"},exportMidi{"Export MIDI"};
    juce::TextButton train{"Train 5 hits"},cancelTrain{"Cancel"},clearTrain{"Reset pads"};
    juce::TextButton drumPads[8];juce::Slider drumNotes[8];
    juce::TextEditor helpText;
    std::vector<std::unique_ptr<SliderAttachment>> sliders;
    std::vector<std::unique_ptr<ButtonAttachment>> buttons;
    std::vector<std::unique_ptr<ComboAttachment>> combos;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::StringArray userNames;
    int factoryCount=0;
    bool calibrating=false;double calibrationStarted=0;float noisePeak=0;
    std::array<float,220> history{};int historyPos=0;
    int previousDrumEvents=0,flashPad=-1,flashTicks=0;
    bool performanceView=true,helpVisible=false,learningRange=false;
    double rangeStarted=0;int learnedLow=127,learnedHigh=0,rangeSamples=0;
    juce::TextButton viewButton{"Studio controls"},saveProfile{"Save voice"},learnRange{"Learn range"},recover{"Recover take"};
    juce::ComboBox harmonyMode,harmonyVoicing,profileSelector;
    juce::ToggleButton bassLayer{"Bass layer"};
    juce::Slider voiceLow,voiceHigh;
    juce::Label arrangementTitle,arrangementStatus,profileTitle,lowLabel,highLabel;
    juce::StringArray profileNames;
    juce::TooltipWindow tooltips{this,500};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IDWVoiceMIDIStudioAudioProcessorEditor)
};
