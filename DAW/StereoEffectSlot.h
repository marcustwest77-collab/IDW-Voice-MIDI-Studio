#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
namespace idw::daw {
// Control-thread lifecycle; the caller must stop callbacks before install/clear.
// This component is not yet connected to Workstation or offline rendering.
class StereoEffectSlot {
public:
    ~StereoEffectSlot(){clear();}
    bool install(std::unique_ptr<juce::AudioProcessor> candidate,double rate,int maximumBlock,juce::String& error){
        error.clear();
        if(!candidate || !std::isfinite(rate) || rate<8000 || rate>768000 || maximumBlock<1 || maximumBlock>65536){
            error="Invalid effect or audio configuration.";return false;
        }
        if(candidate->getBusCount(true)!=1 || candidate->getBusCount(false)!=1){
            error="The first effect slot requires one stereo input and one stereo output bus.";return false;
        }
        auto layout=candidate->getBusesLayout();
        layout.inputBuses.set(0,juce::AudioChannelSet::stereo());
        layout.outputBuses.set(0,juce::AudioChannelSet::stereo());
        if(!candidate->setBusesLayout(layout)){
            error="The effect does not support stereo processing.";return false;
        }
        try{
            candidate->setRateAndBufferSizeDetails(rate,maximumBlock);
            candidate->prepareToPlay(rate,maximumBlock);
        }catch(...){error="The effect failed to prepare.";return false;}
        clear();processor=std::move(candidate);maxBlock=maximumBlock;
        faulted.store(false);bypassed.store(false);return true;
    }
    void clear(){
        if(processor){try{processor->releaseResources();}catch(...){}processor.reset();}
        maxBlock=0;faulted.store(false);midi.clear();
    }
    void setBypassed(bool value){bypassed.store(value,std::memory_order_relaxed);}
    bool hasEffect() const {return processor!=nullptr;}
    // Control thread only, with callbacks stopped and their lock held.
    juce::AudioProcessor* processorForControlThread() const {return processor.get();}
    bool hasFault() const {return faulted.load(std::memory_order_relaxed);}
    int latencySamples() const {return processor?processor->getLatencySamples():0;}
    // In-place, pre-sized stereo buffers only; never create/load/free an instance.
    bool process(juce::AudioBuffer<float>& audio){
        if(!processor || bypassed.load(std::memory_order_relaxed))return true;
        if(faulted.load(std::memory_order_relaxed)){audio.clear();return false;}
        if(audio.getNumChannels()!=2 || audio.getNumSamples()>maxBlock){audio.clear();return false;}
        if(audio.getNumSamples()==0)return true;
        midi.clear();
        try{processor->processBlock(audio,midi);}
        catch(...){audio.clear();faulted.store(true,std::memory_order_relaxed);return false;}
        midi.clear();
        for(int ch=0;ch<2;++ch)for(int i=0;i<audio.getNumSamples();++i)
            if(!std::isfinite(audio.getSample(ch,i))){audio.clear();faulted.store(true,std::memory_order_relaxed);return false;}
        return true;
    }
private:
    std::unique_ptr<juce::AudioProcessor> processor;
    juce::MidiBuffer midi;
    int maxBlock=0;
    std::atomic<bool> bypassed{false},faulted{false};
};
}
