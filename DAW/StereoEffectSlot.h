#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>
#include "PluginStateData.h"
namespace idw::daw {
// Control-thread lifecycle; the caller must stop callbacks before install/clear.
// Playback and offline export each own a separate instance of this slot.
class StereoEffectSlot {
public:
    ~StereoEffectSlot(){clear();}
    bool install(std::unique_ptr<juce::AudioProcessor> candidate,double rate,int maximumBlock,juce::String& error,
                 const PluginStateData* recalled=nullptr,const juce::String& instanceIdentifier={}){
        error.clear();
        if(recalled && !pluginStateMatches(*recalled,instanceIdentifier)){
            error="Plugin identity or saved state is invalid.";return false;
        }
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
            if(recalled)candidate->setStateInformation(recalled->bytes.getData(),(int)recalled->bytes.getSize());
            if(candidate->getBusCount(true)!=1 || candidate->getBusCount(false)!=1 ||
               candidate->getMainBusNumInputChannels()!=2 || candidate->getMainBusNumOutputChannels()!=2){
                candidate->releaseResources();error="Restored effect changed the required stereo layout.";return false;
            }
        }catch(...){
            try{candidate->releaseResources();}catch(...){}
            error="The effect failed to prepare or restore state.";return false;
        }
        clear();processor=std::move(candidate);maxBlock=maximumBlock;
        faulted.store(false);bypassed.store(recalled?recalled->bypassed:false);return true;
    }
    std::optional<PluginStateData> captureState(const juce::String& instanceIdentifier,bool idle,juce::String& error){
        error.clear();
        if(!idle || !processor || instanceIdentifier.isEmpty()){
            error="Stop playback and select the loaded plugin before capturing settings.";return {};
        }
        PluginStateData state;state.identifier=instanceIdentifier;state.bypassed=bypassed.load(std::memory_order_relaxed);
        try{processor->getStateInformation(state.bytes);}
        catch(...){error="The plugin could not provide settings.";return {};}
        if(!validPluginState(state)){error="Plugin settings exceed the supported size or identity limits.";return {};}
        return state;
    }
    bool restoreCandidate(std::unique_ptr<juce::AudioProcessor> candidate,const juce::String& actualIdentifier,
                          const PluginStateData& state,double rate,int block,bool idle,juce::String& error){
        if(!idle){error="Stop playback before restoring plugin settings.";return false;}
        return install(std::move(candidate),rate,block,error,&state,actualIdentifier);
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
