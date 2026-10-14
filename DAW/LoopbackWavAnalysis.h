#pragma once
#include "LatencyAlignment.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

namespace idw::daw {
// Analyze a pre-recorded mono or multichannel WAV on the message/test thread.
// No hardware input or output is opened, and session trim is never modified.
struct WavLoopbackResult {
    RepeatedPulseAlignment alignment;
    double sampleRate=0.0;
    double lagMilliseconds=0.0;
};
inline WavLoopbackResult analyzeLoopbackWav(const juce::File& file,
                                            int channel,
                                            const std::vector<std::size_t>& expectedPulses,
                                            std::size_t searchRadius) {
    WavLoopbackResult result;
    juce::WavAudioFormat format;
    auto reader=std::unique_ptr<juce::AudioFormatReader>(
        format.createReaderFor(file.createInputStream().release(),true));
    if(!reader || reader->sampleRate<=0 || channel<0 ||
       channel>=static_cast<int>(reader->numChannels) ||
       reader->lengthInSamples<=0 || reader->lengthInSamples>48000LL*60*10)
        return result;
    result.sampleRate=reader->sampleRate;
    const auto frames=static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> samples(static_cast<int>(reader->numChannels),frames);
    if(!reader->read(&samples,0,frames,0,true,true))return result;
    result.alignment=estimateLoopbackLatency(samples.getReadPointer(channel),
        static_cast<std::size_t>(frames),expectedPulses,searchRadius);
    if(result.alignment.valid)
        result.lagMilliseconds=alignmentMilliseconds(result.alignment.lagSamples,result.sampleRate);
    return result;
}
}
