#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <vector>
#include <cstddef>
#include <cmath>

namespace idw::daw {
// Produce a reference WAV containing short, safe-amplitude pulses.
// Use an audio interface's supported line-level loopback path, never speaker outputs.
inline bool writeLoopbackReference(const juce::File& file,double sampleRate,
                                   const std::vector<double>& pulseTimesMs,
                                   float amplitude=0.25f) {
    if(!std::isfinite(sampleRate) || sampleRate<8000.0 || sampleRate>192000.0 ||
       pulseTimesMs.size()<3 || pulseTimesMs.size()>64 || !std::isfinite(amplitude) ||
       amplitude<=0.0f || amplitude>0.5f || file.existsAsFile())return false;
    double previous=-1.0;
    for(const auto time:pulseTimesMs){
        if(!std::isfinite(time) || time<0.0 || time<=previous || time>60000.0)return false;
        previous=time;
    }
    const auto frames=static_cast<int>(std::ceil((previous+250.0)*sampleRate/1000.0));
    if(frames<=0 || frames>192000*61)return false;
    auto stream=std::unique_ptr<juce::FileOutputStream>(file.createOutputStream());
    if(!stream)return false;
    juce::WavAudioFormat wav;
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(
        wav.createWriterFor(stream.get(),sampleRate,1,16,{},0));
    if(!writer){stream.reset();file.deleteFile();return false;}
    stream.release();
    juce::AudioBuffer<float> buffer(1,frames);
    buffer.clear();
    for(const auto ms:pulseTimesMs){
        const auto index=static_cast<int>(std::llround(ms*sampleRate/1000.0));
        if(index>=0 && index<frames)buffer.setSample(0,index,amplitude);
    }
    const float* channels[]{buffer.getReadPointer(0)};
    const bool saved=writer->writeFromFloatArrays(channels,1,frames);
    writer.reset();
    if(!saved)file.deleteFile();
    return saved;
}
}
