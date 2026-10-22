#pragma once
#include "VocalTuner.h"
#include <cstdint>

namespace idw {
struct AudioHarmonyIntervals{int first=0,second=0,voices=0;};
inline int scaleStep(int note,int root,std::uint16_t mask,int direction,int steps){
    if(mask==0)mask=1;
    int found=0,current=note;
    while(found<steps&&current+direction>=0&&current+direction<=127){current+=direction;if(scaleContains(current,root,mask))++found;}
    return current-note;
}
inline AudioHarmonyIntervals scaleAwareHarmony(int note,int root,std::uint16_t mask,int style){
    if(note<0||note>127)return {};
    switch(style){
        case 0:return {scaleStep(note,root,mask,1,2),0,1};
        case 1:return {scaleStep(note,root,mask,-1,2),scaleStep(note,root,mask,1,2),2};
        case 3:return {note>=12?-12:12,note<=115?12:-12,2};
        default:return {scaleStep(note,root,mask,1,2),scaleStep(note,root,mask,1,4),2};
    }
}
} // namespace idw
