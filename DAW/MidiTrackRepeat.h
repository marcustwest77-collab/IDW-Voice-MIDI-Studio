#pragma once
#include "SessionModel.h"
namespace idw::daw {
enum class MidiTrackRepeatResult {invalid,unchanged,changed};
// Repeat from beat zero, preserving leading rests. Round the part's latest
// endpoint up to a whole bar; an exact bar endpoint needs no extra bar.
inline MidiTrackRepeatResult repeatMidiTrack(const std::vector<MidiNote>& notes,int totalPlays,
    int numerator,int denominator,std::vector<MidiNote>& output){
    if((totalPlays!=1 && totalPlays!=2 && totalPlays!=4 && totalPlays!=8) ||
       numerator<1 || numerator>32 || denominator<1 || denominator>32 ||
       notes.size()>100000/static_cast<size_t>(totalPlays))return MidiTrackRepeatResult::invalid;
    double latest=0;
    for(const auto& note:notes){
        const double end=note.startBeat+note.lengthBeats;
        if(note.pitch<0 || note.pitch>127 || note.velocity<1 || note.velocity>127 ||
           !std::isfinite(note.startBeat) || note.startBeat<0 || !std::isfinite(note.lengthBeats) || note.lengthBeats<=0 ||
           !std::isfinite(end) || end<=note.startBeat)return MidiTrackRepeatResult::invalid;
        latest=std::max(latest,end);
    }
    if(notes.empty() || totalPlays==1)return MidiTrackRepeatResult::unchanged;
    const double bar=static_cast<double>(numerator)*4.0/denominator;
    const double span=std::ceil(latest/bar)*bar;
    if(!std::isfinite(span) || span<latest || span<=0)return MidiTrackRepeatResult::invalid;
    auto next=notes;next.reserve(notes.size()*static_cast<size_t>(totalPlays));
    for(int play=1;play<totalPlays;++play){
        const double offset=span*play;
        if(!std::isfinite(offset))return MidiTrackRepeatResult::invalid;
        for(const auto& original:notes){
            auto note=original;note.startBeat+=offset;const double end=note.startBeat+note.lengthBeats;
            if(!std::isfinite(note.startBeat) || note.startBeat<=original.startBeat ||
               !std::isfinite(end) || end<=note.startBeat)return MidiTrackRepeatResult::invalid;
            next.push_back(note);
        }
    }
    output=std::move(next);return MidiTrackRepeatResult::changed;
}
}
