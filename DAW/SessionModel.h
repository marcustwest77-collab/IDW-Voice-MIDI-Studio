#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
namespace idw::daw {
enum class TrackType { audio, midi };
struct Clip {
    std::string sourcePath;
    double startBeat=0.0;
    double lengthBeats=0.0;
    double sourceOffsetSeconds=0.0;
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
    float vocalFxMix=1.0f; // Dry/wet blend, 0..1.
    int vocalFxPreset=0; // Audio insert: Off, Gentle, Warm, Forward.
    int synthPreset=0; // Keys=0, Bass=1, Lead=2, Organ=3, Pad=4
    float synthAttackMs=5.0f,synthReleaseMs=5.0f;
    std::vector<Clip> clips;
    std::vector<MidiNote> notes;
};
class Session {
public:
    std::string lyrics;
    double bpm=120.0;
    int numerator=4, denominator=4;
    std::vector<Track> tracks;
    bool addTrack(TrackType type) {
        if (tracks.size() >= 128) return false;
        tracks.push_back({(type == TrackType::audio ? "Audio " : "MIDI ") + std::to_string(tracks.size()+1),type});
        return true;
    }
    bool setTrackVocalFxMix(size_t index,float mix) {
        if(index>=tracks.size() || tracks[index].type!=TrackType::audio ||
           !std::isfinite(mix) || mix<0 || mix>1)return false;
        tracks[index].vocalFxMix=mix;return true;
    }
    bool setTrackVocalFxPreset(size_t index,int preset) {
        if(index>=tracks.size() || tracks[index].type!=TrackType::audio || preset<0 || preset>3)return false;
        tracks[index].vocalFxPreset=preset;return true;
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
            clip.startBeat<0 || clip.lengthBeats<=0 || clip.sourceOffsetSeconds<0) return false;
        tracks[trackIndex].clips.push_back(std::move(clip));
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
