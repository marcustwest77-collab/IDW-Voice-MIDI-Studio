#pragma once
#include "SessionModel.h"
namespace idw::daw {
enum class MidiTransposeResult {invalid,unchanged,changed};
// All-or-nothing: never clamp or drop notes at the MIDI pitch boundaries.
inline MidiTransposeResult transposeMidiNotes(const std::vector<MidiNote>& notes,int semitones,std::vector<MidiNote>& output){
    if(semitones<-127 || semitones>127 || notes.size()>100000)return MidiTransposeResult::invalid;
    auto next=notes;
    for(auto& note:next){
        const auto pitch=static_cast<std::int64_t>(note.pitch)+semitones;
        const double end=note.startBeat+note.lengthBeats;
        if(note.pitch<0 || note.pitch>127 || pitch<0 || pitch>127 || note.velocity<1 || note.velocity>127 ||
           !std::isfinite(note.startBeat) || note.startBeat<0 || !std::isfinite(note.lengthBeats) || note.lengthBeats<=0 ||
           !std::isfinite(end) || end<=note.startBeat)return MidiTransposeResult::invalid;
        note.pitch=static_cast<int>(pitch);
    }
    if(notes.empty() || semitones==0)return MidiTransposeResult::unchanged;
    output=std::move(next);return MidiTransposeResult::changed;
}
}
