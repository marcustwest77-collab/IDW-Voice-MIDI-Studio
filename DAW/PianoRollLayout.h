#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace idw::daw {
// Retain the familiar position in large windows; move upward only when needed
// to keep a usable editor and its footer inside a shorter content area.
inline juce::Rectangle<int> pianoRollOverlayBounds(int width,int height){
    if(width<=0 || height<=0)return {};
    const int horizontal=std::min(20,width/10),vertical=std::min(20,height/10);
    const int top=std::min(335,std::max(vertical,height-vertical-240));
    return {horizontal,top,width-2*horizontal,std::min(480,height-vertical-top)};
}
}
