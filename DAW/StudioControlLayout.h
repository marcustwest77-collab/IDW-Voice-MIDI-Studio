#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
namespace idw::daw {
enum StudioControlRow {transportRow,projectRow,mixRow,monitorRow,statusRow,
    overdubRow,masterRow,synthRow,eqRow,ceilingRow,studioRowCount};
// The processing deck has unused space to the right of the synth/tone controls.
// Keep command feedback here rather than beyond the right edge of the toolbar.
inline std::array<juce::Rectangle<int>,5> studioCommandBounds(int width){
    std::array<juce::Rectangle<int>,5> result{};
    if(width<1240)return result; // Allow for native window borders at the minimum size.
    juce::Rectangle<int> row(620,250,width-640,28);
    result[3]=row.removeFromRight(100).reduced(3);
    result[2]=row.removeFromRight(85).reduced(3);
    result[1]=row.removeFromRight(95).reduced(3);
    result[0]=row.reduced(3);
    result[4]={623,280,width-646,40};
    return result;
}
inline std::array<juce::Rectangle<int>,studioRowCount> studioControlRows(int width){
    std::array<juce::Rectangle<int>,studioRowCount> rows{};
    if(width<=40)return rows;
    constexpr int heights[]{32,30,30,22,20,26,28,24,24,24};
    int top=62; // 20-pixel margin plus the 42-pixel title/status header.
    for(size_t i=0;i<rows.size();++i){rows[i]={20,top,width-40,heights[i]};top+=heights[i];}
    return rows; // Last control ends at 322; timeline ruler begins at 330.
}
}
