#pragma once
#include <JuceHeader.h>
#include "YinPitchDetector.h"
#include "ScaleManager.h"
#include "PresetManager.h"
#include "MPEAllocator.h"
#include "MidiLearnManager.h"
#include "BeatboxClassifier.h"
#include "PerformanceCapture.h"
class IDWVoiceMIDIStudioAudioProcessor : public juce::AudioProcessor {
public:
    IDWVoiceMIDIStudioAudioProcessor();
    void prepareToPlay(double,int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override{return true;}
    const juce::String getName() const override{return "IDW Voice MIDI Studio";}
    bool acceptsMidi() const override{return true;}
    bool producesMidi() const override{return true;}
    bool isMidiEffect() const override{return false;}
    double getTailLengthSeconds() const override{return 0;}
    int getNumPrograms() override{return 1;}
    int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{}
    const juce::String getProgramName(int) override{return {};}
    void changeProgramName(int,const juce::String&) override{}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;
    MidiLearnManager learn;
    BeatboxClassifier beats;
    PerformanceCapture capture;
    void requestPanic(){panicRequested.store(true);}
    void requestTestNote(){testRequested.store(true);}
    void saveExtraState();
    void restoreExtraState();
    unsigned int audioCallbacks() const{return callbacks.load();}
    float peak() const{return inputPeak.load();}
    double deviceRate() const{return observedRate.load();}
    int deviceBlock() const{return observedBlock.load();}
    float hz() const{return freq.load();}
    int note() const{return midi.load();}
    float conf() const{return confidence.load();}
    float level() const{return inputRms.load();}
    float bendPosition() const{return displayedBend.load();}
    int drumEventCount() const{return drumEvents.load();}
    int lastDrum() const{return drumDisplay.load();}
    int noteCount() const{return emittedNotes.load();}
    int eventCount() const{return emittedEvents.load();}
    double callbackLoad() const{return cpuLoad.load();}
private:
    float value(const char* id) const{return apvts.getRawParameterValue(id)->load();}
    void analyse(juce::MidiBuffer&,int);
    void endVoice(juce::MidiBuffer&,int);
    void allOff(juce::MidiBuffer&,int);
    void negotiateBend(juce::MidiBuffer&,int,int,int);
    YinPitchDetector pitch;
    MPEAllocator mpe;
    ScaleManager scale;
    std::vector<float> hop;
    int hopSize=240,hopFill=0;
    double sampleRateHz=48000;
    int active=-1,activeChannel=1,candidate=-1,candidateSamples=0,silentSamples=0,ccSamples=0,lastCC=-1,lastWheel=-1;
    bool previousMpe=false;
    int previousFirst=2,previousLast=16;
    float smoothNote=-1,lastHz=0;
    std::array<int,17> negotiatedRanges{};
    std::array<int,128> drumRemaining{};
    int testRemaining=0,inhibitSamples=0;
    double phase=0;float previewGain=0;
    std::atomic<bool> panicRequested{false},testRequested{false};
    std::atomic<float> freq{0},confidence{0},inputRms{0},displayedBend{0};
    std::atomic<int> midi{-1},drumDisplay{-1},drumEvents{0},emittedNotes{0},emittedEvents{0};
    std::atomic<double> cpuLoad{0},observedRate{0};
    std::atomic<unsigned int> callbacks{0};
    std::atomic<int> observedBlock{0};
    std::atomic<float> inputPeak{0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IDWVoiceMIDIStudioAudioProcessor)
};
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
