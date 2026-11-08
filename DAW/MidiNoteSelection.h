#pragma once
#include "SessionModel.h"
#include <optional>
#include <tuple>
namespace idw::daw {
// Read-only chronological traversal: start, pitch, then original list index.
// Wrap at either end without sorting or changing the stored notes.
inline std::optional<size_t> adjacentMidiNote(const std::vector<MidiNote>& notes,std::optional<size_t> current,bool forward){
    if(notes.empty() || notes.size()>100000 || (current && *current>=notes.size()))return {};
    for(const auto& n:notes){const double end=n.startBeat+n.lengthBeats;
        if(n.pitch<0 || n.pitch>127 || n.velocity<1 || n.velocity>127 || !std::isfinite(n.startBeat) || n.startBeat<0 ||
           !std::isfinite(n.lengthBeats) || n.lengthBeats<=0 || !std::isfinite(end) || end<=n.startBeat)return {};
    }
    const auto key=[&](size_t i){return std::make_tuple(notes[i].startBeat,notes[i].pitch,i);};
    size_t edge=0;std::optional<size_t> next;
    for(size_t i=0;i<notes.size();++i){
        if(forward ? key(i)<key(edge) : key(i)>key(edge))edge=i;
        if(!current)continue;
        const bool eligible=forward ? key(i)>key(*current) : key(i)<key(*current);
        if(eligible && (!next || (forward ? key(i)<key(*next) : key(i)>key(*next))))next=i;
    }
    return next ? next : std::optional<size_t>{edge};
}
}
