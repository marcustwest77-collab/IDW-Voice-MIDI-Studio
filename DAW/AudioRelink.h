#pragma once
#include "SessionModel.h"
#include <juce_audio_formats/juce_audio_formats.h>
namespace idw::daw {
inline std::vector<std::string> missingAudioSources(const Session& session,juce::AudioFormatManager& formats){
    std::vector<std::string> seen,missing;
    for(const auto& track:session.tracks)if(track.type==TrackType::audio)
        for(const auto& clip:track.clips){
            if(std::find(seen.begin(),seen.end(),clip.sourcePath)!=seen.end())continue;
            seen.push_back(clip.sourcePath);
            if(!juce::File::isAbsolutePath(juce::String(clip.sourcePath))){
                missing.push_back(clip.sourcePath);continue;
            }
            auto reader=std::unique_ptr<juce::AudioFormatReader>(
                formats.createReaderFor(juce::File(juce::String(clip.sourcePath))));
            if(!reader || !std::isfinite(reader->sampleRate) || reader->sampleRate<=0 ||
               reader->numChannels==0 || reader->lengthInSamples<=0)missing.push_back(clip.sourcePath);
        }
    return missing;
}
// Prepare a complete candidate; never change live state or files on failure.
inline bool prepareAudioRelink(const Session& current,const std::string& missingPath,
                               const juce::File& replacement,juce::AudioFormatManager& formats,
                               Session& output,juce::String& error){
    const auto missing=missingAudioSources(current,formats);
    if(std::find(missing.begin(),missing.end(),missingPath)==missing.end()){
        error="This source is no longer missing. Refresh the missing-audio list.";return false;
    }
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(replacement));
    if(!reader || !std::isfinite(reader->sampleRate) || reader->sampleRate<=0 ||
       reader->numChannels==0 || reader->lengthInSamples<=0){
        error="Choose a readable, non-empty audio file.";return false;
    }
    if(!std::isfinite(current.bpm) || current.bpm<=0){
        error="The project tempo is invalid.";return false;
    }
    const double duration=(double)reader->lengthInSamples/reader->sampleRate;
    size_t count=0;
    for(const auto& track:current.tracks)if(track.type==TrackType::audio)
        for(const auto& clip:track.clips)if(clip.sourcePath==missingPath){
            const double end=clip.sourceOffsetSeconds+clip.lengthBeats*60.0/current.bpm;
            if(!std::isfinite(end) || end<0 || end>duration+0.5/reader->sampleRate){
                error="This file is too short for one or more clips, including their source offsets.";return false;
            }
            ++count;
        }
    if(count==0){error="No clips reference this source.";return false;}
    Session candidate=current;
    for(auto& track:candidate.tracks)if(track.type==TrackType::audio)
        for(auto& clip:track.clips)if(clip.sourcePath==missingPath)
            clip.sourcePath=replacement.getFullPathName().toStdString();
    output=std::move(candidate);error.clear();return true;
}
}
