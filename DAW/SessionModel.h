#pragma once
#include <algorithm>
#include <string>
#include <vector>
namespace idw::daw {
enum class TrackType { audio, midi };
struct Track { std::string name; TrackType type; bool mute=false; bool solo=false; };
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
    void setTempo(double value) { bpm=std::clamp(value,20.0,300.0); }
    double secondsPerBeat() const { return 60.0/bpm; }
};
}
