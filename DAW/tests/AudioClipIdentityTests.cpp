#include "AudioClipIdentity.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static const juce::Colour background(0xff17394c),edge(0xffeaf6ff),dark(0xff07111d);
static void drawClip(juce::Graphics& g,int x,int width,int top,size_t number){
    g.setColour(background);g.fillRect(x,top,width,52);
    g.setColour(juce::Colour(0xff6a9db4));
    for(int i=3;i<width-3;++i){const float height=3.0f+9.0f*std::abs(std::sin((float)(x+i)*0.21f));
        g.drawVerticalLine(x+i,(float)top+34-height,(float)top+34+height);}
    drawAudioClipIdentity(g,{x,top,width,52},number);
}
int main(int argc,char** argv){try{
    juce::ScopedJuceInitialiser_GUI init;
    require(audioClipBadgeWidth(2)==0 && audioClipBadgeWidth(23)==0 && audioClipBadgeWidth(24)>0,"narrow clip badge bounds");
    require(audioClipBadgeText(2,36)=="C2" && audioClipBadgeText(3,120)=="Clip 3","clip number labels");
    juce::Image image(juce::Image::ARGB,240,80,true,juce::SoftwareImageType());juce::Graphics g(image);
    g.fillAll(juce::Colours::black);
    drawClip(g,20,36,10,1);drawClip(g,56,48,10,2);drawClip(g,104,120,10,3);
    for(int x:{20,55,56,103,104,223})if(image.getPixelAt(x,40)!=edge)throw std::runtime_error("edge "+std::to_string(x)+" got "+image.getPixelAt(x,40).toString().toStdString());
    for(int x:{21,54,57,102,105,222})require(image.getPixelAt(x,40)==dark,"edge contrast backing missing");
    require(image.getPixelAt(25,13)==dark && image.getPixelAt(109,13)==dark,"badge opaque backing missing");
    require(image.getPixelAt(19,40)==juce::Colours::black && image.getPixelAt(224,40)==juce::Colours::black,"identity painting leaked outside clip");
    juce::Image scrolled(juce::Image::ARGB,140,70,true,juce::SoftwareImageType());juce::Graphics scroll(scrolled);
    scroll.fillAll(background);scroll.reduceClipRegion(10,0,90,70);
    const auto priorClip=scroll.getClipBounds();
    drawAudioClipIdentity(scroll,{-30,10,120,52},7);
    require(scroll.getClipBounds()==priorClip,"graphics clip state not restored");
    require(scrolled.getPixelAt(10,40)==background && scrolled.getPixelAt(89,40)==edge &&
        scrolled.getPixelAt(99,40)==background && scrolled.getPixelAt(9,40)==background,"offscreen edge invented or viewport leaked");
    for(int width:{1,2,3,8,23}){
        juce::Image narrow(juce::Image::ARGB,40,70,true,juce::SoftwareImageType());juce::Graphics ng(narrow);ng.fillAll(background);
        drawAudioClipIdentity(ng,{10,10,width,52},1);
        require(narrow.getPixelAt(10,40)==edge && narrow.getPixelAt(9,40)==background &&
            narrow.getPixelAt(10+width,40)==background,"tiny clip edge/clipping");
    }
    juce::Image empty(juce::Image::ARGB,20,20,true,juce::SoftwareImageType());juce::Graphics eg(empty);eg.fillAll(background);
    drawAudioClipIdentity(eg,{},1);require(empty.getPixelAt(0,0)==background,"empty bounds changed image");
    if(argc==2){
        juce::Image preview(juce::Image::RGB,900,290,true,juce::SoftwareImageType());juce::Graphics pg(preview);pg.fillAll(juce::Colour(0xff080f1c));
        pg.setColour(juce::Colours::white);pg.setFont(juce::Font(juce::FontOptions(14.0f)));
        pg.drawText("Audio clip boundaries: 3 / 4 / 42 beats at normal and larger zoom",20,8,860,24,juce::Justification::centredLeft);
        drawClip(pg,32,36,42,1);drawClip(pg,68,48,42,2);drawClip(pg,116,507,42,3);
        drawClip(pg,44,72,116,1);drawClip(pg,116,96,116,2);drawClip(pg,212,650,116,3);
        {juce::Graphics::ScopedSaveState state(pg);pg.reduceClipRegion(20,190,860,70);
            drawClip(pg,-30,200,190,2);drawClip(pg,170,650,190,3);}
        const juce::File output(argv[1]);require(!output.exists(),"preview output already exists");
        auto stream=output.createOutputStream();require(stream!=nullptr,"preview stream");juce::PNGImageFormat png;
        require(png.writeImageToStream(preview,*stream),"preview write");
    }
    std::cout<<"Audio clip identity rendering tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
