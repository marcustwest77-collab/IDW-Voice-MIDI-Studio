#pragma once
#include <juce_core/juce_core.h>
#include "SessionModel.h"

// Minimal metadata only: never export clip paths, audio buffers or recordings.
// This snapshot is an explicit transfer format, not an authentication mechanism.
namespace idw::daw {
inline juce::var aiSessionStatus(const Session& session,bool playing,bool recording) {
    auto root=std::make_unique<juce::DynamicObject>();
    root->setProperty("format","idw-ai-status-v1");
    root->setProperty("bpm",session.bpm);
    root->setProperty("numerator",session.numerator);
    root->setProperty("denominator",session.denominator);
    root->setProperty("playing",playing);
    root->setProperty("recording",recording);
    juce::Array<juce::var> tracks;
    for(const auto& t:session.tracks){
        auto obj=std::make_unique<juce::DynamicObject>();
        obj->setProperty("name",juce::String(t.name));
        obj->setProperty("type",t.type==TrackType::audio?"audio":"midi");
        obj->setProperty("gain",t.gain);
        obj->setProperty("pan",t.pan);
        obj->setProperty("mute",t.mute);
        obj->setProperty("solo",t.solo);
        obj->setProperty("clipCount",(int)t.clips.size());
        tracks.add(juce::var(obj.release()));
    }
    root->setProperty("tracks",juce::var(tracks));
    return juce::var(root.release());
}
}
