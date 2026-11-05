#pragma once
#include "SessionModel.h"

namespace idw::daw {
enum class MidiQuantizeResult {invalid,unchanged,changed};
// Move starts only. Half-grid ties move forward; order, lengths and expression
// remain unchanged, including notes that overlap after snapping.
inline MidiQuantizeResult quantizeMidiNotes(const std::vector<MidiNote>& notes,int divisions,std::vector<MidiNote>& output){
    if((divisions!=1 && divisions!=2 && divisions!=3 && divisions!=4 && divisions!=6 && divisions!=8) || notes.size()>100000)
        return MidiQuantizeResult::invalid;
    auto next=notes;bool changed=false;
    for(auto& note:next){
        const double scaled=note.startBeat*divisions,oldEnd=note.startBeat+note.lengthBeats;
        if(note.pitch<0 || note.pitch>127 || note.velocity<1 || note.velocity>127 ||
           !std::isfinite(note.startBeat) || note.startBeat<0 || !std::isfinite(note.lengthBeats) || note.lengthBeats<=0 ||
           !std::isfinite(oldEnd) || oldEnd<=note.startBeat || !std::isfinite(scaled) || scaled>4503599627370495.0)
            return MidiQuantizeResult::invalid;
        const double start=std::floor(scaled+0.5)/divisions,end=start+note.lengthBeats;
        if(!std::isfinite(start) || !std::isfinite(end) || end<=start)return MidiQuantizeResult::invalid;
        changed=changed || start!=note.startBeat;note.startBeat=start;
    }
    if(!changed)return MidiQuantizeResult::unchanged;
    output=std::move(next);return MidiQuantizeResult::changed;
}
}
