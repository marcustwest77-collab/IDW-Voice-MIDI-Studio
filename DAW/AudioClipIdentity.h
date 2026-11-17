#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace idw::daw {
inline int audioClipBadgeWidth(int clipWidth){
    return clipWidth>=58?std::min(70,clipWidth-10):(clipWidth>=24?clipWidth-10:0);
}
inline juce::String audioClipBadgeText(size_t number,int clipWidth){
    return (clipWidth>=58?juce::String("Clip "):juce::String("C"))+juce::String(std::to_string(number));
}
// Paint only inside the real clip bounds and the caller's visible lane. Never
// pin an offscreen edge to the viewport: that would invent a split location.
inline void drawAudioClipIdentity(juce::Graphics& g,juce::Rectangle<int> clip,size_t number){
    if(clip.isEmpty())return;
    juce::Graphics::ScopedSaveState state(g);
    if(!g.reduceClipRegion(clip))return;
    const int darkWidth=std::min(3,clip.getWidth());
    g.setColour(juce::Colour(0xff07111d));
    g.fillRect(clip.getX(),clip.getY(),darkWidth,clip.getHeight());
    g.fillRect(clip.getRight()-darkWidth,clip.getY(),darkWidth,clip.getHeight());
    g.setColour(juce::Colour(0xffeaf6ff));
    g.fillRect(clip.getX(),clip.getY(),1,clip.getHeight());
    if(clip.getWidth()>2)g.fillRect(clip.getRight()-1,clip.getY(),1,clip.getHeight());
    const int badgeWidth=audioClipBadgeWidth(clip.getWidth());
    if(badgeWidth<=0 || clip.getHeight()<20)return;
    const juce::Rectangle<int> badge(clip.getX()+5,clip.getY()+3,badgeWidth,14);
    g.setColour(juce::Colour(0xff07111d));g.fillRect(badge);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(10.0f,juce::Font::bold)));
    g.drawFittedText(audioClipBadgeText(number,clip.getWidth()),badge.reduced(2,0),juce::Justification::centredLeft,1);
}
}
