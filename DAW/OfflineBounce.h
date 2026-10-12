#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "SessionModel.h"
#include <algorithm>
#include <cmath>
#include <atomic>
namespace idw::daw {
// Offline stereo bounce. Reads source files into small blocks and sums against
// the shared beat clock. Intended for PCM WAV/AIFF project clips, not MIDI yet.
// Call only from a non-audio thread. Returns false and a readable error.
inline bool renderSessionWav(const Session& session,const juce::File& destination,
                            juce::String& error,int sampleRate=44100,float masterGain=1.0f,float ceilingDb=-1.0f,float lowDb=0.0f,float highDb=0.0f,
                            std::atomic<float>* progress=nullptr,std::atomic<bool>* cancel=nullptr){
    if(session.bpm<=0 || sampleRate<8000 || sampleRate>192000 || !std::isfinite(masterGain) || !std::isfinite(ceilingDb) || !std::isfinite(lowDb) || !std::isfinite(highDb)){
        error="Invalid bounce rate or tempo.";return false;
    }
    double lastBeat=0.0;
    for(const auto& t:session.tracks)
        for(const auto& c:t.clips)lastBeat=std::max(lastBeat,c.startBeat+c.lengthBeats);
    if(lastBeat<=0){error="Session has no clips.";return false;}
    const int64_t frames=(int64_t)std::ceil(lastBeat*60.0/session.bpm*sampleRate);
    if(frames<=0 || frames>(int64_t)sampleRate*60*60*4){
        error="Export duration must be under four hours.";return false;
    }
    if(destination.existsAsFile()){error="Choose a new WAV filename.";return false;}
    auto stream=std::unique_ptr<juce::FileOutputStream>(destination.createOutputStream());
    if(!stream){error="Cannot create WAV output.";return false;}
    juce::WavAudioFormat wav;
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(
        wav.createWriterFor(stream.get(),(double)sampleRate,2,24,{},0));
    if(!writer){error="Cannot create WAV writer.";destination.deleteFile();return false;}
    stream.release();
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    constexpr int block=1024;
    juce::AudioBuffer<float> mix(2,block),scratch(2,block);
    const float* channels[2]={mix.getReadPointer(0),mix.getReadPointer(1)};
    const bool anySolo=std::any_of(session.tracks.begin(),session.tracks.end(),
        [](const Track& t){return t.solo;});
    // Open sources once, before rendering.
    struct ActiveClip {
        std::unique_ptr<juce::AudioFormatReader> reader;
        int64_t start=0,end=0,offset=0;
        double sourceRate=44100.0;
        float left=1,right=1;
    };
    std::vector<ActiveClip> clips;
    for(const auto& track:session.tracks){
        if(track.type!=TrackType::audio || track.mute || (anySolo&&!track.solo))continue;
        for(const auto& clip:track.clips){
            auto reader=std::unique_ptr<juce::AudioFormatReader>(
                formats.createReaderFor(juce::File(juce::String(clip.sourcePath))));
            if(!reader){error="Missing or unsupported audio: "+juce::String(clip.sourcePath);writer.reset();destination.deleteFile();return false;}

            const int64_t start=(int64_t)std::llround(clip.startBeat*60.0/session.bpm*sampleRate);
            const int64_t length=(int64_t)std::llround(clip.lengthBeats*60.0/session.bpm*sampleRate);
            const float pan=std::clamp(track.pan,-1.0f,1.0f),gain=std::clamp(track.gain,0.0f,2.0f);
            const double sourceRate=reader->sampleRate;
            clips.push_back({std::move(reader),start,start+length,
                (int64_t)std::llround(clip.sourceOffsetSeconds*sourceRate),sourceRate,
                gain*(pan>0?1-pan:1),gain*(pan<0?1+pan:1)});
        }
    }
    const float outputCeiling=std::pow(10.0f,std::clamp(ceilingDb,-12.0f,-0.1f)/20.0f);
    const float outputGain=std::clamp(masterGain,0.0f,1.5f);
    const float alpha=(float)std::exp(-2.0*juce::MathConstants<double>::pi*500.0/sampleRate);
    const float bassGain=std::pow(10.0f,std::clamp(lowDb,-12.0f,12.0f)/20.0f);
    const float trebleGain=std::pow(10.0f,std::clamp(highDb,-12.0f,12.0f)/20.0f);
    float filterState[2]{0.0f,0.0f};
    if(progress)progress->store(0.0f,std::memory_order_relaxed);
    for(int64_t pos=0;pos<frames;pos+=block){
        if(cancel && cancel->load(std::memory_order_relaxed)){
            error="Export canceled.";writer.reset();destination.deleteFile();return false;
        }
        const int count=(int)std::min<int64_t>(block,frames-pos);
        mix.clear();
        for(auto& clip:clips){
            const int64_t begin=std::max(pos,clip.start),end=std::min(pos+count,clip.end);
            if(end<=begin)continue;
            const int offset=(int)(begin-pos),number=(int)(end-begin);
            // Linear interpolation at the source sample rate, with one extra frame
            // to interpolate across export block boundaries.
            const double ratio=clip.sourceRate/sampleRate;
            const double first=clip.offset+(begin-clip.start)*ratio;
            const int64_t sourceStart=(int64_t)std::floor(first);
            const int sourceCount=(int)std::ceil(first-sourceStart+(number-1)*ratio)+2;
            // Reuse the scratch buffer across clips and blocks rather than allocating
            // a temporary AudioBuffer for every overlap.
            if(scratch.getNumSamples()<sourceCount)
                scratch.setSize(2,sourceCount,false,false,true);
            scratch.clear(0,sourceCount);
            clip.reader->read(&scratch,0,sourceCount,sourceStart,true,true);
            for(int i=0;i<number;++i){
                const double at=first-sourceStart+i*ratio;
                const int index=(int)std::floor(at);
                const float fraction=(float)(at-index);
                const float l=scratch.getSample(0,index)*(1.0f-fraction)+scratch.getSample(0,index+1)*fraction;
                const int right=clip.reader->numChannels>1?1:0;
                const float r=scratch.getSample(right,index)*(1.0f-fraction)+scratch.getSample(right,index+1)*fraction;
                mix.addSample(0,offset+i,l*clip.left);
                mix.addSample(1,offset+i,r*clip.right);
            }
        }
        for(int ch=0;ch<2;++ch){
            auto* p=mix.getWritePointer(ch);
            float state=filterState[ch];
            for(int i=0;i<count;++i){
                const float input=p[i];
                state=alpha*state+(1.0f-alpha)*input;
                const float shaped=bassGain*state+trebleGain*(input-state);
                p[i]=std::clamp(shaped*outputGain,-outputCeiling,outputCeiling);
            }
            filterState[ch]=state;
        }
        if(!writer->writeFromFloatArrays(channels,2,count)){
            error="Failed while writing WAV samples.";writer.reset();destination.deleteFile();return false;
        }
        if(progress)progress->store((float)(pos+count)/(float)frames,std::memory_order_relaxed);
    }
    writer.reset();return true;
}
}
