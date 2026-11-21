#include "DimensionalControls.h"
#include <iostream>
#include <stdexcept>
#include <limits>
static void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
static juce::Image render(bool enabled,bool pressed,bool panel=false){
    juce::SoftwareImageType software;juce::Image result(juce::Image::ARGB,80,40,true,software);
    juce::Graphics g(result);
    const juce::Rectangle<float> bounds(8,6,64,28);
    if(panel)idw::daw::drawStudioPanel(g,bounds,juce::Colour(0xff203447),juce::Colours::cyan);
    else idw::daw::drawRaisedControl(g,bounds,juce::Colour(0xff203447),juce::Colours::cyan,enabled,false,pressed);
    return result;
}
static void testFaders(){
    juce::SoftwareImageType software;
    for(bool horizontal:{false,true}){
        const juce::Rectangle<float> bounds=horizontal?juce::Rectangle<float>(8,8,64,24):juce::Rectangle<float>(8,8,24,64);
        const float start=8,end=72;
        juce::Image states[4];
        for(int mode=0;mode<4;++mode){
            states[mode]=juce::Image(juce::Image::ARGB,80,80,true,software);
            juce::Graphics g(states[mode]);
            idw::daw::drawStudioFader(g,bounds,mode==1?end:start,horizontal,juce::Colours::cyan,mode!=2,false,mode==3);
        }
        int changes[3]{},painted=0;
        for(int y=0;y<80;++y)for(int x=0;x<80;++x){
            if(states[0].getPixelAt(x,y).getAlpha())++painted;
            for(int i=0;i<4;++i){
                if(!bounds.contains((float)x,(float)y))require(states[i].getPixelAt(x,y).getAlpha()==0,"fader escaped bounds");
                if(i>0 && states[i].getPixelAt(x,y)!=states[0].getPixelAt(x,y))++changes[i-1];
            }
        }
        require(painted>100 && changes[0]>100 && changes[1]>100 && changes[2]>50,"fader position/disabled/focus not distinct");
    }
    juce::Image clipped(juce::Image::ARGB,80,40,true,software);
    {juce::Graphics g(clipped);g.reduceClipRegion(20,10,10,10);
        const auto before=g.getClipBounds();
        idw::daw::drawStudioFader(g,{8,8,64,24},-100,true,juce::Colours::cyan,true,true,true);
        require(g.getClipBounds()==before,"fader changed caller clip");}
    for(int y=0;y<40;++y)for(int x=0;x<80;++x)
        if(x<20 || x>=30 || y<10 || y>=20)require(clipped.getPixelAt(x,y).getAlpha()==0,"fader ignored inherited clip");
    juce::Image empty(juce::Image::ARGB,20,20,true,software);
    {juce::Graphics g(empty);
        idw::daw::drawStudioFader(g,{0,0,2,2},1,true,juce::Colours::cyan,true,false,false);
        idw::daw::drawStudioFader(g,{0,0,20,20},std::numeric_limits<float>::quiet_NaN(),true,juce::Colours::cyan,true,false,false);}
    for(int y=0;y<20;++y)for(int x=0;x<20;++x)require(empty.getPixelAt(x,y).getAlpha()==0,"invalid fader drew pixels");
}
int main(){try{
    testFaders();
    const auto normal=render(true,false),down=render(true,true),disabled=render(false,false),panel=render(true,false,true);
    int pressedChanges=0,disabledChanges=0,painted=0;
    for(int y=0;y<40;++y)for(int x=0;x<80;++x){
        const auto pixel=normal.getPixelAt(x,y);
        if(pixel.getAlpha()>0)++painted;
        if(pixel!=down.getPixelAt(x,y))++pressedChanges;
        if(pixel!=disabled.getPixelAt(x,y))++disabledChanges;
        if(x<8 || x>=72 || y<6 || y>=34)
            require(pixel.getAlpha()==0 && down.getPixelAt(x,y).getAlpha()==0 &&
                disabled.getPixelAt(x,y).getAlpha()==0 && panel.getPixelAt(x,y).getAlpha()==0,"drawing escaped control bounds");
    }
    require(painted>100 && pressedChanges>100 && disabledChanges>100,"states not visibly distinct");
    juce::SoftwareImageType software;juce::Image image(juce::Image::ARGB,30,30,true,software);
    {
        juce::Graphics g(image);g.reduceClipRegion(juce::Rectangle<int>(10,10,5,5));
        idw::daw::drawRaisedControl(g,{0,0,30,30},juce::Colours::grey,juce::Colours::cyan,true,false,false);
    }
    for(int y=0;y<30;++y)for(int x=0;x<30;++x)
        if(x<10 || x>=15 || y<10 || y>=15)require(image.getPixelAt(x,y).getAlpha()==0,"inherited clip lost");
    juce::Image empty(juce::Image::ARGB,10,10,true,software);{
        juce::Graphics g(empty);
        idw::daw::drawRaisedControl(g,{0,0,0,0},juce::Colours::grey,juce::Colours::cyan,true,false,false);
        idw::daw::drawStudioPanel(g,{0,0,2,2},juce::Colours::grey,juce::Colours::cyan);
    }
    require(empty.getPixelAt(0,0).getAlpha()==0,"tiny bounds drawing");
    std::cout<<"Dimensional controls tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
