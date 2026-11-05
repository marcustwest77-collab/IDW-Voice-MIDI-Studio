#pragma once
#include "SessionModel.h"
namespace idw::daw {
enum class MidiTrackVelocityMode {offset,fixed};
enum class MidiTrackVelocityResult {invalid,unchanged,changed};
// Validate the complete part before publishing; never clamp velocities or create note-offs.
inline MidiTrackVelocityResult adjustMidiTrackVelocity(const std::vector<MidiNote>& notes,
    MidiTrackVelocityMode mode,int value,std::vector<MidiNote>& output){
    if(notes.size()>100000 ||
       (mode==MidiTrackVelocityMode::offset ? value<-126 || value>126 :
        mode==MidiTrackVelocityMode::fixed ? value<1 || value>127 : true))return MidiTrackVelocityResult::invalid;
    auto next=notes;bool changed=false;
    for(auto& note:next){
        const auto velocity=mode==MidiTrackVelocityMode::fixed ? static_cast<std::int64_t>(value) :
            static_cast<std::int64_t>(note.velocity)+value;
        const double end=note.startBeat+note.lengthBeats;
        if(note.pitch<0 || note.pitch>127 || note.velocity<1 || note.velocity>127 || velocity<1 || velocity>127 ||
           !std::isfinite(note.startBeat) || note.startBeat<0 || !std::isfinite(note.lengthBeats) || note.lengthBeats<=0 ||
           !std::isfinite(end) || end<=note.startBeat)return MidiTrackVelocityResult::invalid;
        changed=changed || velocity!=note.velocity;note.velocity=static_cast<int>(velocity);
    }
    if(!changed)return MidiTrackVelocityResult::unchanged;
    output=std::move(next);return MidiTrackVelocityResult::changed;
}
}
