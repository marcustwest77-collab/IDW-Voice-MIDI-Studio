#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <optional>
namespace idw::daw {
// Inspect the completed file on the export worker, never on the audio thread.
inline std::optional<juce::String> wavExportReceipt(const juce::File& sourceProject,
                                                   const juce::File& output){
    juce::WavAudioFormat wav;
    auto stream=output.createInputStream();
    if(!stream)return {};
    auto reader=std::unique_ptr<juce::AudioFormatReader>(
        wav.createReaderFor(stream.release(),true));
    if(!reader || !std::isfinite(reader->sampleRate) || reader->sampleRate<=0 ||
       reader->lengthInSamples<=0 || reader->numChannels==0 || reader->bitsPerSample==0)return {};
    const double seconds=(double)reader->lengthInSamples/reader->sampleRate;
    if(!std::isfinite(seconds))return {};
    const auto source=sourceProject==juce::File{}?juce::String("Untitled project"):sourceProject.getFullPathName();
    return "Source project (current session snapshot):\n"+source+
        "\n\nWAV file:\n"+output.getFullPathName()+
        "\n\nDuration: "+juce::String(seconds,3)+" seconds\n"+
        juce::String(reader->numChannels)+" channels | "+juce::String(reader->bitsPerSample)+
        "-bit | "+juce::String(reader->sampleRate,0)+" Hz";
}
}
