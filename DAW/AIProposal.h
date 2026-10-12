#pragma once
#include <juce_core/juce_core.h>
#include "SessionModel.h"
#include <cmath>

namespace idw::daw {
// Approved actions are intentionally limited to non-destructive changes.
// Previewed JSON proposals cannot access recording files or execute commands.
struct AIProposal {
    enum class Kind { tempo, addAudioTrack, addMidiTrack };
    Kind kind;
    double bpm=120.0;
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
    return false;
}

inline bool applyApprovedAIProposal(Session& session,const AIProposal& proposal) {
    switch(proposal.kind){
        case AIProposal::Kind::tempo: session.setTempo(proposal.bpm);return true;
        case AIProposal::Kind::addAudioTrack: return session.addTrack(TrackType::audio);
        case AIProposal::Kind::addMidiTrack: return session.addTrack(TrackType::midi);
    }
    return false;
}
}
