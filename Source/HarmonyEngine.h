#pragma once
#include <array>
#include <algorithm>
#include <cstdlib>
namespace idw {
struct HarmonyNotes {
    std::array<int,4> chord{{-1,-1,-1,-1}};
    int bass=-1;
    bool operator==(const HarmonyNotes& o) const { return chord==o.chord && bass==o.bass; }
    bool operator!=(const HarmonyNotes& o) const { return !(*this==o); }
};
// Fixed-size, allocation-free diatonic harmonizer. Mode 1 = major, 2 = natural minor.
inline HarmonyNotes harmonize(int lead,int root,int mode,bool seventh,bool bassEnabled) {
    HarmonyNotes out;
    if(lead<0 || lead>127 || mode<1 || mode>2) return out;
    constexpr int major[]{0,2,4,5,7,9,11},minor[]{0,2,3,5,7,8,10};
    const int* scale=mode==1?major:minor;root=((root%12)+12)%12;
    int anchor=0,degree=0,distance=999;
    for(int octave=-1;octave<11;++octave) for(int d=0;d<7;++d){
        const int candidate=root+12*octave+scale[d];
        if(candidate>=0&&candidate<=127&&std::abs(candidate-lead)<distance){anchor=candidate;degree=d;distance=std::abs(candidate-lead);}
    }
    for(int i=0;i<(seventh?4:3);++i){
        const int d=degree+i*2;
        const int note=anchor+scale[d%7]-scale[degree]+12*(d/7);
        // Omit notes above MIDI range rather than duplicate a clamped top note.
        if(note<=127)out.chord[(size_t)i]=note;
    }
    if(bassEnabled){out.bass=anchor-24;while(out.bass<0)out.bass+=12;}
    return out;
}
}
