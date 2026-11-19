#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
namespace idw::daw {
// One reusable bus, prepared outside callbacks. Finish each track before reuse.
class TrackAudioBus {
public:
    bool prepare(int maximumBlock){
        if(maximumBlock<1 || maximumBlock>65536)return false;
        storage.setSize(2,maximumBlock);capacity=maximumBlock;frames=0;return true;
    }
    bool begin(int count){
        frames=0;if(count<1 || count>capacity)return false;
        frames=count;storage.clear(0,count);return true;
    }
    bool add(const juce::AudioBuffer<float>& source,int sourceOffset,int destinationOffset,int count){
        if(frames==0 || source.getNumChannels()<2 || count<0 || sourceOffset<0 || destinationOffset<0 ||
           sourceOffset>source.getNumSamples()-count || destinationOffset>frames-count)return false;
        for(int ch=0;ch<2;++ch)storage.addFrom(ch,destinationOffset,source,ch,sourceOffset,count);
        return true;
    }
    // Process the complete summed track once, before track gain/pan. The view
    // refers to preallocated samples; the two channel pointers fit inline.
    template<class Effect>
    float mix(juce::AudioBuffer<float>& output,int offset,float gain,float pan,Effect&& effect){
        if(frames==0 || offset<0 || offset>output.getNumSamples()-frames ||
           output.getNumChannels()<1 || output.getNumChannels()>2 || !std::isfinite(gain) || !std::isfinite(pan))return 0;
        float* channels[]{storage.getWritePointer(0),storage.getWritePointer(1)};
        juce::AudioBuffer<float> view(channels,2,frames);
        if(!effect(view))return 0;
        gain=juce::jlimit(0.0f,2.0f,gain);pan=juce::jlimit(-1.0f,1.0f,pan);
        const float levels[]{gain*(pan>0?1-pan:1),gain*(pan<0?1+pan:1)};
        float peak=0;
        for(int ch=0;ch<2;++ch)peak=juce::jmax(peak,storage.getMagnitude(ch,0,frames)*levels[ch]);
        for(int ch=0;ch<output.getNumChannels();++ch)output.addFrom(ch,offset,storage,ch,0,frames,levels[ch]);
        frames=0;return peak;
    }
private:
    juce::AudioBuffer<float> storage;int capacity=0,frames=0;
};
}
