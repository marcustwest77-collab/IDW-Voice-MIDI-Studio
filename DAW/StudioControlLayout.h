#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
namespace idw::daw {
enum StudioControlRow {transportRow,projectRow,mixRow,monitorRow,statusRow,
    overdubRow,masterRow,synthRow,eqRow,ceilingRow,studioRowCount};
inline std::array<juce::Rectangle<int>,studioRowCount> studioControlRows(int width){
    std::array<juce::Rectangle<int>,studioRowCount> rows{};
    if(width<=40)return rows;
    constexpr int heights[]{32,30,30,22,20,26,28,24,24,24};
    int top=62; // 20-pixel margin plus the 42-pixel title/status header.
    for(size_t i=0;i<rows.size();++i){rows[i]={20,top,width-40,heights[i]};top+=heights[i];}
    return rows; // Last control ends at 322; timeline ruler begins at 330.
}
}
