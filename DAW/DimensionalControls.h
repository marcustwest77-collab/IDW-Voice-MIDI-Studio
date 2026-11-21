#pragma once
#include <juce_graphics/juce_graphics.h>
namespace idw::daw {
inline void drawRaisedControl(juce::Graphics& g,juce::Rectangle<float> bounds,juce::Colour base,
                              juce::Colour accent,bool enabled,bool hovered,bool pressed,float radius=6.0f){
    if(bounds.getWidth()<4 || bounds.getHeight()<4)return;
    const juce::Graphics::ScopedSaveState state(g);g.reduceClipRegion(bounds.getSmallestIntegerContainer());
    auto face=bounds.reduced(1.0f);face.removeFromBottom(pressed?1.0f:3.0f);
    if(pressed)face=face.translated(0,1);
    if(face.isEmpty())return;
    radius=juce::jmin(radius,face.getHeight()/2);
    if(!enabled){base=base.withSaturation(0.1f).darker(0.25f);accent=accent.withSaturation(0.1f);}
    if(hovered && enabled)base=base.brighter(0.15f);
    g.setColour(juce::Colours::black.withAlpha(enabled?0.55f:0.25f));
    g.fillRoundedRectangle(face.translated(0,pressed?1.0f:3.0f),radius);
    const auto top=pressed?base.darker(0.2f):base.brighter(0.28f);
    const auto bottom=pressed?base.brighter(0.1f):base.darker(0.25f);
    g.setGradientFill(juce::ColourGradient(top,face.getCentreX(),face.getY(),bottom,face.getCentreX(),face.getBottom(),false));
    g.fillRoundedRectangle(face,radius);
    g.setColour(juce::Colours::black.withAlpha(0.6f));g.drawRoundedRectangle(face,radius,1);
    g.setColour(accent.withAlpha(enabled?(pressed?0.65f:0.24f):0.08f));
    g.drawRoundedRectangle(face.reduced(0.5f),radius,1);
    if(face.getWidth()>8 && face.getHeight()>5){
        g.setColour(juce::Colours::white.withAlpha(enabled?(pressed?0.06f:0.20f):0.06f));
        g.drawHorizontalLine((int)face.getY()+1,face.getX()+radius,face.getRight()-radius);
    }
}
inline void drawStudioPanel(juce::Graphics& g,juce::Rectangle<float> bounds,juce::Colour base,juce::Colour accent){
    if(bounds.getWidth()<4 || bounds.getHeight()<4)return;
    const juce::Graphics::ScopedSaveState state(g);g.reduceClipRegion(bounds.getSmallestIntegerContainer());
    const auto area=bounds.reduced(0.5f);const float radius=juce::jmin(9.0f,area.getHeight()/2);
    g.setGradientFill(juce::ColourGradient(base.brighter(0.18f),area.getX(),area.getY(),base.darker(0.18f),area.getRight(),area.getBottom(),false));
    g.fillRoundedRectangle(area,radius);
    g.setColour(juce::Colours::black.withAlpha(0.75f));g.drawRoundedRectangle(area,radius,1);
    g.setColour(accent.withAlpha(0.23f));g.drawRoundedRectangle(area.reduced(0.8f),radius,1);
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawHorizontalLine((int)area.getY()+1,area.getX()+radius,area.getRight()-radius);
}
}
