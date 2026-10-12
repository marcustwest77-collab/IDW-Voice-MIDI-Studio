#pragma once
#include <juce_core/juce_core.h>
#include "SessionModel.h"
#include <cmath>

namespace idw::daw {
// Approved actions are intentionally limited to non-destructive changes.
// Previewed JSON proposals cannot access recording files or execute commands.
struct AIProposal {
    enum class Kind { tempo, addAudioTrack, addMidiTrack, setTrackMix };
    Kind kind;
    double bpm=120.0;
    size_t trackIndex=0;
    float gain=1.0f,pan=0.0f;
};

inline bool parseAIProposal(const juce::var& value, AIProposal& proposal) {
    auto* obj=value.getDynamicObject();
    if(!obj || obj->getProperties().size()!=2 ||
       obj->getProperty("format").toString()!="idw-ai-proposal-v1")
        return false;
    const auto action=obj->getProperty("action");
    auto* spec=action.getDynamicObject();
    if(!spec)return false;
    const auto type=spec->getProperty("type").toString();
    if(type=="set_tempo" && spec->getProperties().size()==2){
        const auto raw=spec->getProperty("bpm");
        if(!raw.isDouble() && !raw.isInt() && !raw.isInt64())return false;
        const double bpm=(double)raw;
        if(!std::isfinite(bpm) || bpm<20.0 || bpm>300.0)return false;
        proposal={AIProposal::Kind::tempo,bpm};return true;
    }
    if(type=="add_track" && spec->getProperties().size()==2){
        const auto trackType=spec->getProperty("track_type").toString();
        if(trackType=="audio"){proposal={AIProposal::Kind::addAudioTrack};return true;}
        if(trackType=="midi"){proposal={AIProposal::Kind::addMidiTrack};return true;}
    }
    if(type=="set_track_mix" && spec->getProperties().size()==4){
        const auto track=spec->getProperty("track_index");
        const auto gain=spec->getProperty("gain");
        const auto pan=spec->getProperty("pan");
        if((!track.isInt() && !track.isInt64()) ||
           (!gain.isDouble() && !gain.isInt() && !gain.isInt64()) ||
           (!pan.isDouble() && !pan.isInt() && !pan.isInt64()))return false;
        const auto i=(juce::int64)track;
        const double g=(double)gain,p=(double)pan;
        if(i<0 || i>=128 || !std::isfinite(g) || !std::isfinite(p) ||
           g<0.0 || g>2.0 || p< -1.0 || p>1.0)return false;
        proposal={AIProposal::Kind::setTrackMix,120.0,(size_t)i,(float)g,(float)p};
        return true;
    }
    return false;
}

inline bool applyApprovedAIProposal(Session& session,const AIProposal& proposal) {
    switch(proposal.kind){
        case AIProposal::Kind::tempo: session.setTempo(proposal.bpm);return true;
        case AIProposal::Kind::addAudioTrack: return session.addTrack(TrackType::audio);
        case AIProposal::Kind::addMidiTrack: return session.addTrack(TrackType::midi);
        case AIProposal::Kind::setTrackMix: return session.setTrackMix(proposal.trackIndex,proposal.gain,proposal.pan);
    }
    return false;
}
}
