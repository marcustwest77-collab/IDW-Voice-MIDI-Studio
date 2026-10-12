#pragma once
#include <JuceHeader.h>
#include "SessionModel.h"
#include <functional>

// First graphical MIDI editor: notes can be drawn with a click and selected.
// MIDI playback/virtual instruments are not yet connected.
class IDWPianoRoll final : public juce::Component {
public:
    idw::daw::Session* session=nullptr;
    size_t trackIndex=0;
    std::function<void()> onChanged;
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff141b24));
        const auto bounds=getLocalBounds();
        constexpr int keyWidth=54,rowHeight=13;
        for(int pitch=84;pitch>=48;--pitch){
            const int y=(84-pitch)*rowHeight;
            g.setColour((pitch%12==1||pitch%12==3||pitch%12==6||
                         pitch%12==8||pitch%12==10)?juce::Colour(0xff1e2631):
                                                        juce::Colour(0xff29323e));
            g.fillRect(0,y,bounds.getWidth(),rowHeight-1);
            g.setColour(juce::Colours::lightgrey);
            if(pitch%12==0)g.drawText("C"+juce::String(pitch/12-1),2,y,48,rowHeight,juce::Justification::centredLeft);
        }
        for(int beat=0;beat<64;++beat){
            const int x=keyWidth+beat*16;
            if(x>getWidth())break;
            g.setColour(beat%4==0?juce::Colours::grey:juce::Colour(0xff435064));
            g.drawVerticalLine(x,0,(float)getHeight());
        }
        if(!session || trackIndex>=session->tracks.size())return;
        for(const auto& note:session->tracks[trackIndex].notes){
            if(note.pitch<48 || note.pitch>84)continue;
            const int y=(84-note.pitch)*rowHeight;
            const int x=keyWidth+juce::roundToInt(note.startBeat*16.0);
            const int w=juce::jmax(3,juce::roundToInt(note.lengthBeats*16.0));
            g.setColour(juce::Colours::orange);
            g.fillRoundedRectangle((float)x,(float)y+1,(float)w,rowHeight-3,2.0f);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if(!session || trackIndex>=session->tracks.size() ||
           session->tracks[trackIndex].type!=idw::daw::TrackType::midi ||
           e.x<54 || e.y<0 || e.y>=37*13)return;
        const int pitch=84-e.y/13;
        const double beat=std::max(0.0,std::round(((double)e.x-54)/16.0*4.0)/4.0);
        if(session->addMidiNote(trackIndex,{pitch,beat,1.0,100})){
            if(onChanged)onChanged();
            repaint();
        }
    }
};
