#pragma once
#include "SessionModel.h"
#include <juce_core/juce_core.h>
#include <tuple>

namespace idw::daw {
// Compare persisted fields directly: no JSON allocation or disk I/O in the UI poll.
inline bool sameDocumentState(const Session& a,const Session& b){
    if(std::tie(a.playbackLoopEnabled,a.playbackLoop.startBeat,a.playbackLoop.endBeat)!=
       std::tie(b.playbackLoopEnabled,b.playbackLoop.startBeat,b.playbackLoop.endBeat))return false;
    if(std::tie(a.bpm,a.numerator,a.denominator,a.masterMix.gain,a.masterMix.ceilingDb,a.masterMix.lowDb,a.masterMix.highDb)!=
       std::tie(b.bpm,b.numerator,b.denominator,b.masterMix.gain,b.masterMix.ceilingDb,b.masterMix.lowDb,b.masterMix.highDb) ||
       a.tracks.size()!=b.tracks.size())return false;
    for(size_t i=0;i<a.tracks.size();++i){
        const auto& x=a.tracks[i];const auto& y=b.tracks[i];
        if(std::tie(x.name,x.type,x.mute,x.solo,x.gain,x.pan)!=std::tie(y.name,y.type,y.mute,y.solo,y.gain,y.pan) ||
           x.clips.size()!=y.clips.size() || x.notes.size()!=y.notes.size())return false;
        if(x.type==TrackType::audio && x.inputChannel!=y.inputChannel)return false;
        if(x.type==TrackType::midi && std::tie(x.synthPreset,x.synthAttackMs,x.synthReleaseMs)!=
            std::tie(y.synthPreset,y.synthAttackMs,y.synthReleaseMs))return false;
        for(size_t c=0;c<x.clips.size();++c){
            const auto& p=x.clips[c];const auto& q=y.clips[c];
            if(std::tie(p.sourcePath,p.startBeat,p.lengthBeats,p.sourceOffsetSeconds,p.fadeInBeats,p.fadeOutBeats,p.fadeInCurve,p.fadeOutCurve,p.gainDb)!=
               std::tie(q.sourcePath,q.startBeat,q.lengthBeats,q.sourceOffsetSeconds,q.fadeInBeats,q.fadeOutBeats,q.fadeInCurve,q.fadeOutCurve,q.gainDb))return false;
        }
        for(size_t n=0;n<x.notes.size();++n){
            const auto& p=x.notes[n];const auto& q=y.notes[n];
            if(std::tie(p.pitch,p.startBeat,p.lengthBeats,p.velocity)!=std::tie(q.pitch,q.startBeat,q.lengthBeats,q.velocity))return false;
        }
    }
    return true;
}
class ProjectDocumentStatus {
public:
    // Called for startup/new, a successful open/recovery, or a successful manual save.
    // Recovery checkpoints deliberately do not mark a document saved.
    void begin(const Session& session,const juce::File& path,bool recovered=false){
        baseline=session;project=path;needsRecoverySave=recovered;
    }
    bool hasUnsavedChanges(const Session& session) const {return needsRecoverySave || !sameDocumentState(session,baseline);}
    juce::String summary(const Session& session) const {
        const auto name=project==juce::File{}?juce::String("Untitled project"):project.getFileName();
        const auto state=needsRecoverySave?"Recovered - save required":
            hasUnsavedChanges(session)?"Unsaved changes":project==juce::File{}?"Not saved yet":"Saved";
        return name+" | "+state;
    }
    juce::String location() const {
        return project==juce::File{}?juce::String("No project file yet. Use SAVE PROJECT to choose a filename."):
            project.getFullPathName();
    }
private:
    Session baseline;
    juce::File project;
    bool needsRecoverySave=false;
};
}
