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
struct Track {
    std::string name;
    TrackType type;
    bool mute=false;
    bool solo=false;
    std::vector<Clip> clips;
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
            clip.startBeat<0 || clip.lengthBeats<=0 || clip.sourceOffsetSeconds<0) return false;
        tracks[trackIndex].clips.push_back(std::move(clip));
        return true;
    }
    void setTempo(double value) { if (std::isfinite(value)) bpm=std::clamp(value,20.0,300.0); }
    double secondsPerBeat() const { return 60.0/bpm; }
};
}
