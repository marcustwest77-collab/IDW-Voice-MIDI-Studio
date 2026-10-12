#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "SessionModel.h"
#include <algorithm>
#include <cmath>
namespace idw::daw {
// Offline stereo bounce. Reads source files into small blocks and sums against
// the shared beat clock. Intended for PCM WAV/AIFF project clips, not MIDI yet.
// Call only from a non-audio thread. Returns false and a readable error.
inline bool renderSessionWav(const Session& session,const juce::File& destination,
                            juce::String& error,int sampleRate=44100){
    if(session.bpm<=0 || sampleRate<8000 || sampleRate>192000){
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
        float left=1,right=1;
    };
    std::vector<ActiveClip> clips;
    for(const auto& track:session.tracks){
        if(track.type!=TrackType::audio || track.mute || (anySolo&&!track.solo))continue;
        for(const auto& clip:track.clips){
            auto reader=std::unique_ptr<juce::AudioFormatReader>(
                formats.createReaderFor(juce::File(juce::String(clip.sourcePath))));
            if(!reader){error="Missing or unsupported audio: "+juce::String(clip.sourcePath);writer.reset();destination.deleteFile();return false;}
            if(std::abs(reader->sampleRate-sampleRate)>0.001){
                error="Bounce requires 44.1 kHz source audio; resampling is not available yet.";
                writer.reset();destination.deleteFile();return false;
            }
            const int64_t start=(int64_t)std::llround(clip.startBeat*60.0/session.bpm*sampleRate);
            const int64_t length=(int64_t)std::llround(clip.lengthBeats*60.0/session.bpm*sampleRate);
            const float pan=std::clamp(track.pan,-1.0f,1.0f),gain=std::clamp(track.gain,0.0f,2.0f);
            clips.push_back({std::move(reader),start,start+length,
                (int64_t)std::llround(clip.sourceOffsetSeconds*sampleRate),
                gain*(pan>0?1-pan:1),gain*(pan<0?1+pan:1)});
        }
    }
    for(int64_t pos=0;pos<frames;pos+=block){
        const int count=(int)std::min<int64_t>(block,frames-pos);
        mix.clear();
        for(auto& clip:clips){
            const int64_t begin=std::max(pos,clip.start),end=std::min(pos+count,clip.end);
            if(end<=begin)continue;
            const int offset=(int)(begin-pos),number=(int)(end-begin);
            scratch.clear();
            clip.reader->read(&scratch,0,number,clip.offset+begin-clip.start,true,true);
            mix.addFrom(0,offset,scratch,0,0,number,clip.left);
            mix.addFrom(1,offset,scratch,clip.reader->numChannels>1?1:0,0,number,clip.right);
        }
        for(int ch=0;ch<2;++ch){
            auto* p=mix.getWritePointer(ch);
            for(int i=0;i<count;++i)p[i]=std::clamp(p[i],-1.0f,1.0f);
        }
        if(!writer->writeFromFloatArrays(channels,2,count)){
            error="Failed while writing WAV samples.";writer.reset();destination.deleteFile();return false;
        }
    }
    writer.reset();return true;
}
}
