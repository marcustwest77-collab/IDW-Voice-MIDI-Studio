#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
namespace idw::daw {
enum class TrackType { audio, midi };
enum class FadeCurve { linear, equalPower };
inline bool validFadeCurve(FadeCurve curve) {
    return curve==FadeCurve::linear || curve==FadeCurve::equalPower;
}
inline bool validClipGain(double db){return std::isfinite(db) && db>=-60.0 && db<=12.0;}
struct Clip {
    std::string sourcePath;
    double startBeat=0.0;
    double lengthBeats=0.0;
    double sourceOffsetSeconds=0.0;
    double fadeInBeats=0.0,fadeOutBeats=0.0;
    FadeCurve fadeInCurve=FadeCurve::linear,fadeOutCurve=FadeCurve::linear;
    double gainDb=0.0; // Per-clip volume, independent of track gain.
    void clampFades() {
        fadeInBeats=std::min(fadeInBeats,lengthBeats);
        fadeOutBeats=std::min(fadeOutBeats,lengthBeats);
    }
};
struct MidiNote { int pitch=60; double startBeat=0.0,lengthBeats=1.0; int velocity=100; };
struct Track {
    std::string name;
    TrackType type;
    bool mute=false;
    bool solo=false;
    int inputChannel=0; // Saved audio input preference, zero-based (0..7).
    float gain=1.0f;
    float pan=0.0f;
    int synthPreset=0; // Keys=0, Bass=1, Lead=2, Organ=3, Pad=4
    float synthAttackMs=5.0f,synthReleaseMs=5.0f;
    std::vector<Clip> clips;
    std::vector<MidiNote> notes;
};
class Session {
public:
    double bpm=120.0;
    int numerator=4, denominator=4;
    std::vector<Track> tracks;
    bool addTrack(TrackType type) {
        if (tracks.size() >= 128) return false;
        tracks.push_back({(type == TrackType::audio ? "Audio " : "MIDI ") + std::to_string(tracks.size()+1),type});
        return true;
    }
    bool removeTrack(size_t index) {
        if(index>=tracks.size()) return false;
        tracks.erase(tracks.begin()+static_cast<std::ptrdiff_t>(index));
        return true;
    }
    bool addClip(size_t trackIndex, Clip clip) {
        if (trackIndex>=tracks.size() || tracks[trackIndex].type!=TrackType::audio ||
            clip.sourcePath.empty() || !std::isfinite(clip.startBeat) ||
            !std::isfinite(clip.lengthBeats) || !std::isfinite(clip.sourceOffsetSeconds) ||
            clip.startBeat<0 || clip.lengthBeats<=0 || clip.sourceOffsetSeconds<0 ||
            !std::isfinite(clip.fadeInBeats) || !std::isfinite(clip.fadeOutBeats) ||
            clip.fadeInBeats<0 || clip.fadeOutBeats<0 ||
            clip.fadeInBeats>clip.lengthBeats || clip.fadeOutBeats>clip.lengthBeats ||
            !validClipGain(clip.gainDb) || !validFadeCurve(clip.fadeInCurve) || !validFadeCurve(clip.fadeOutCurve)) return false;
        tracks[trackIndex].clips.push_back(std::move(clip));
        return true;
    }
    // Copy metadata only; repeat the same source segment after this clip.
    bool duplicateClip(size_t track,size_t index) {
        if(track>=tracks.size() || tracks[track].type!=TrackType::audio ||
           index>=tracks[track].clips.size())return false;
        const auto original=tracks[track].clips[index];
        const double nextStart=original.startBeat+original.lengthBeats;
        const double nextEnd=nextStart+original.lengthBeats;
        if(!std::isfinite(original.startBeat) || original.startBeat<0 ||
           !std::isfinite(original.lengthBeats) || original.lengthBeats<=0 ||
           !std::isfinite(nextStart) || !std::isfinite(nextEnd) ||
           nextStart<=original.startBeat || nextEnd<=nextStart)return false;
        auto copy=original;copy.startBeat=nextStart;
        auto& clips=tracks[track].clips;
        clips.insert(clips.begin()+static_cast<std::ptrdiff_t>(index+1),std::move(copy));
        return true;
    }
    bool setClipGain(size_t track,size_t index,double db) {
        if(track>=tracks.size() || tracks[track].type!=TrackType::audio ||
           index>=tracks[track].clips.size() || !validClipGain(db))return false;
        tracks[track].clips[index].gainDb=db;return true;
    }
    bool setClipFades(size_t track,size_t index,double in,double out,
                      FadeCurve inCurve=FadeCurve::linear,FadeCurve outCurve=FadeCurve::linear) {
        if(track>=tracks.size() || tracks[track].type!=TrackType::audio ||
           index>=tracks[track].clips.size() || !std::isfinite(in) || !std::isfinite(out) ||
           in<0 || out<0 || !validFadeCurve(inCurve) || !validFadeCurve(outCurve))return false;
        auto& clip=tracks[track].clips[index];
        if(in>clip.lengthBeats || out>clip.lengthBeats)return false;
        clip.fadeInBeats=in;clip.fadeOutBeats=out;
        clip.fadeInCurve=inCurve;clip.fadeOutCurve=outCurve;
        return true;
    }
    // Explicit edge fades, not an automatic overlap policy. Ordinary overlaps still sum.
    // Reject containment and third clips in the transition: no ambiguous pair selection.
    bool crossfadeClips(size_t track,size_t first,size_t second,FadeCurve curve) {
        if(track>=tracks.size() || tracks[track].type!=TrackType::audio || first==second ||
           first>=tracks[track].clips.size() || second>=tracks[track].clips.size() ||
           !validFadeCurve(curve))return false;
        auto& clips=tracks[track].clips;
        if(clips[first].startBeat>clips[second].startBeat)std::swap(first,second);
        auto& a=clips[first];auto& b=clips[second];
        const double end=a.startBeat+a.lengthBeats;
        const double overlapBeats=end-b.startBeat;
        if(!(a.startBeat<b.startBeat && overlapBeats>0 && end<b.startBeat+b.lengthBeats))return false;
        // Preserve outside fades without multiplying them into the transition.
        if(a.fadeInBeats>b.startBeat-a.startBeat || b.fadeOutBeats>b.lengthBeats-overlapBeats)return false;
        for(size_t i=0;i<clips.size();++i)
            if(i!=first && i!=second && clips[i].startBeat<end &&
               clips[i].startBeat+clips[i].lengthBeats>b.startBeat)return false;
        a.fadeOutBeats=overlapBeats;b.fadeInBeats=overlapBeats;
        a.fadeOutCurve=curve;b.fadeInCurve=curve;
        return true;
    }
    // Timeline edit primitive used by future drag/snap controls.
    // Validate before mutating so failed edits leave the session unchanged.
    bool moveClip(size_t trackIndex,size_t clipIndex,double newStartBeat) {
        if(trackIndex>=tracks.size() || clipIndex>=tracks[trackIndex].clips.size() ||
           !std::isfinite(newStartBeat) || newStartBeat<0.0)return false;
        tracks[trackIndex].clips[clipIndex].startBeat=newStartBeat;
        return true;
    }
    bool resizeClip(size_t trackIndex,size_t clipIndex,double newLengthBeats) {
        if(trackIndex>=tracks.size() || clipIndex>=tracks[trackIndex].clips.size() ||
           !std::isfinite(newLengthBeats) || newLengthBeats<=0.0)return false;
        tracks[trackIndex].clips[clipIndex].lengthBeats=newLengthBeats;
        tracks[trackIndex].clips[clipIndex].clampFades();
        return true;
    }
    // Clip metadata only: source WAVs are never modified.
    bool splitClip(size_t track,size_t index,double beat) {
        if(track>=tracks.size() || index>=tracks[track].clips.size() || !std::isfinite(beat))return false;
        auto& clips=tracks[track].clips;
        const auto original=clips[index];
        const double first=beat-original.startBeat;
        if(first<=1.0e-6 || first>=original.lengthBeats-1.0e-6 ||
           first<original.fadeInBeats || first>original.lengthBeats-original.fadeOutBeats)return false;
        auto second=original;
        second.startBeat=beat;
        second.lengthBeats-=first;
        second.sourceOffsetSeconds+=first*secondsPerBeat();
        second.fadeInBeats=0;
        clips[index].fadeOutBeats=0;
        clips[index].lengthBeats=first;
        clips.insert(clips.begin()+static_cast<std::ptrdiff_t>(index+1),second);
        return true;
    }
    bool trimClipLeft(size_t track,size_t index,double beat) {
        if(track>=tracks.size() || index>=tracks[track].clips.size() || !std::isfinite(beat))return false;
        auto& clip=tracks[track].clips[index];
        const double delta=beat-clip.startBeat;
        if(delta<0 || delta>=clip.lengthBeats-1.0e-6)return false;
        clip.startBeat=beat;clip.lengthBeats-=delta;
        clip.sourceOffsetSeconds+=delta*secondsPerBeat();
        clip.clampFades();
        return true;
    }
    bool trimClipRight(size_t track,size_t index,double beat) {
        if(track>=tracks.size() || index>=tracks[track].clips.size() || !std::isfinite(beat))return false;
        auto& clip=tracks[track].clips[index];
        const double newLength=beat-clip.startBeat;
        if(newLength<=1.0e-6 || newLength>clip.lengthBeats)return false;
        clip.lengthBeats=newLength;
        clip.clampFades();
        return true;
    }
    bool addMidiNote(size_t trackIndex,MidiNote note) {
        if(trackIndex>=tracks.size() || tracks[trackIndex].type!=TrackType::midi ||
           tracks[trackIndex].notes.size()>=100000 || note.pitch<0 || note.pitch>127 ||
           note.velocity<1 || note.velocity>127 || !std::isfinite(note.startBeat) ||
           !std::isfinite(note.lengthBeats) || note.startBeat<0 || note.lengthBeats<=0)return false;
        tracks[trackIndex].notes.push_back(note);
        return true;
    }
    bool removeMidiNote(size_t trackIndex,size_t noteIndex) {
        if(trackIndex>=tracks.size() || tracks[trackIndex].type!=TrackType::midi ||
           noteIndex>=tracks[trackIndex].notes.size())return false;
        auto& notes=tracks[trackIndex].notes;
        notes.erase(notes.begin()+static_cast<std::ptrdiff_t>(noteIndex));
        return true;
    }
    bool moveMidiNote(size_t trackIndex,size_t noteIndex,double startBeat,int pitch) {
        if(trackIndex>=tracks.size() || noteIndex>=tracks[trackIndex].notes.size() ||
           !std::isfinite(startBeat) || startBeat<0 || pitch<0 || pitch>127)return false;
        auto& note=tracks[trackIndex].notes[noteIndex];
        note.startBeat=startBeat;note.pitch=pitch;
        return true;
    }
    bool resizeMidiNote(size_t trackIndex,size_t noteIndex,double lengthBeats) {
        if(trackIndex>=tracks.size() || noteIndex>=tracks[trackIndex].notes.size() ||
           !std::isfinite(lengthBeats) || lengthBeats<=0)return false;
        tracks[trackIndex].notes[noteIndex].lengthBeats=lengthBeats;
        return true;
    }
    bool setTrackSynthEnvelope(size_t index,float attackMs,float releaseMs) {
        if(index>=tracks.size() || tracks[index].type!=TrackType::midi ||
           !std::isfinite(attackMs) || !std::isfinite(releaseMs) ||
           attackMs<0 || attackMs>2000 || releaseMs<0 || releaseMs>2000)return false;
        tracks[index].synthAttackMs=attackMs;
        tracks[index].synthReleaseMs=releaseMs;
        return true;
    }
    bool setTrackSynthPreset(size_t index,int preset) {
        if(index>=tracks.size() || tracks[index].type!=TrackType::midi || preset<0 || preset>4)return false;
        tracks[index].synthPreset=preset;
        return true;
    }
    bool setTrackMix(size_t index,float gain,float pan) {
        if(index>=tracks.size() || !std::isfinite(gain) || !std::isfinite(pan))return false;
        tracks[index].gain=std::clamp(gain,0.0f,2.0f);
        tracks[index].pan=std::clamp(pan,-1.0f,1.0f);
        return true;
    }
    void setTempo(double value) { if (std::isfinite(value)) bpm=std::clamp(value,20.0,300.0); }
    double secondsPerBeat() const { return 60.0/bpm; }
};
}
