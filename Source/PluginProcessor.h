#pragma once
#include <JuceHeader.h>
#include "YinPitchDetector.h"
#include "BeatboxClassifier.h"
#include "ScaleManager.h"
#include "MPEAllocator.h"
#include "MidiLearnManager.h"
#include "PresetManager.h"
class IDWVoiceMIDIStudioAudioProcessor:public juce::AudioProcessor{
public:IDWVoiceMIDIStudioAudioProcessor();void prepareToPlay(double,int)override;void releaseResources()override;bool isBusesLayoutSupported(const BusesLayout&)const override;void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&)override;
juce::AudioProcessorEditor*createEditor()override;bool hasEditor()const override{return true;}const juce::String getName()const override{return "IDW Voice MIDI Studio";}bool acceptsMidi()const override{return true;}bool producesMidi()const override{return true;}
bool isMidiEffect()const override{return false;}double getTailLengthSeconds()const override{return 0;}int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return{};}void changeProgramName(int,const juce::String&)override{}
void getStateInformation(juce::MemoryBlock&)override;void setStateInformation(const void*,int)override;juce::AudioProcessorValueTreeState apvts;PresetManager presets;MidiLearnManager learn;ScaleManager scale;
float hz()const{return freq.load();}int note()const{return midi.load();}float conf()const{return confidence.load();}static juce::AudioProcessorValueTreeState::ParameterLayout layout();
private:YinPitchDetector pitch;BeatboxClassifier beats;MPEAllocator mpe;std::atomic<float>freq{0},confidence{0};std::atomic<int>midi{-1};int active=-1,silent=0;float lastHz=0;
JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IDWVoiceMIDIStudioAudioProcessor)};
juce::AudioProcessor*JUCE_CALLTYPE createPluginFilter();
