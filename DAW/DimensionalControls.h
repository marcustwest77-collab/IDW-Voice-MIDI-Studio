#pragma once
#include <juce_graphics/juce_graphics.h>
#include <cmath>
namespace idw::daw {
// Position is the JUCE-provided pixel centre, not a remapping of the value.
inline void drawStudioFader(juce::Graphics& g,juce::Rectangle<float> bounds,float position,
                            bool horizontal,juce::Colour accent,bool enabled,bool active,bool focused){
    if(bounds.getWidth()<8 || bounds.getHeight()<8 || !std::isfinite(position))return;
    const juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(bounds.getSmallestIntegerContainer());
    const auto centre=bounds.getCentre();
    auto rail=horizontal?juce::Rectangle<float>(bounds.getX(),centre.y-3,bounds.getWidth(),6)
                        :juce::Rectangle<float>(centre.x-3,bounds.getY(),6,bounds.getHeight());
    if(!enabled)accent=accent.withSaturation(0.0f).withMultipliedAlpha(0.35f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));g.fillRoundedRectangle(rail.translated(0,1),3);
    g.setColour(juce::Colour(0xff080e16));g.fillRoundedRectangle(rail,3);
    const float low=horizontal?bounds.getX():bounds.getY();
    const float high=horizontal?bounds.getRight():bounds.getBottom();
    position=juce::jlimit(low,high,position);
    auto fill=rail.reduced(1);
    if(horizontal)fill.setRight(juce::jmax(fill.getX(),position));
    else fill.setTop(juce::jmin(fill.getBottom(),position));
    g.setColour(accent.withMultipliedAlpha(0.65f));g.fillRoundedRectangle(fill,2);
    const float capLength=juce::jmin(12.0f,high-low);
    const float capCross=juce::jmin(18.0f,horizontal?bounds.getHeight():bounds.getWidth());
    const float capStart=juce::jlimit(low,high-capLength,position-capLength/2);
    const auto cap=horizontal?juce::Rectangle<float>(capStart,centre.y-capCross/2,capLength,capCross)
                             :juce::Rectangle<float>(centre.x-capCross/2,capStart,capCross,capLength);
    g.setColour(juce::Colours::black.withAlpha(0.6f));g.fillRoundedRectangle(cap.translated(0,2),2);
    const auto top=juce::Colour(enabled?(active?0xfff0f6fc:0xffced9e4):0xff66717b);
    g.setGradientFill(juce::ColourGradient(top,cap.getX(),cap.getY(),
        juce::Colour(enabled?0xff526577:0xff39434d),cap.getX(),cap.getBottom(),false));
    g.fillRoundedRectangle(cap,2);
    g.setColour(juce::Colours::black.withAlpha(0.8f));g.drawRoundedRectangle(cap.reduced(0.5f),2,1);
    g.setColour(accent);
    if(horizontal)g.fillRect(cap.getCentreX()-0.75f,cap.getY()+3,1.5f,cap.getHeight()-6);
    else g.fillRect(cap.getX()+3,cap.getCentreY()-0.75f,cap.getWidth()-6,1.5f);
    if(focused && enabled){g.setColour(accent);g.drawRoundedRectangle(bounds.reduced(0.75f),3,1.5f);}
}
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
