#include "PianoRoll.h"
#include "PianoRollLayout.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    juce::ScopedJuceInitialiser_GUI init;
    require(pianoRollOverlayBounds(1450,850)==juce::Rectangle<int>(20,335,1410,480),"large window geometry changed");
    require(pianoRollOverlayBounds(1280,720)==juce::Rectangle<int>(20,335,1240,365),"short window footer clipped");
    require(pianoRollOverlayBounds(800,480)==juce::Rectangle<int>(20,220,760,240),"compact editor did not move upward");
    for(int width:{0,-1,std::numeric_limits<int>::min()})require(pianoRollOverlayBounds(width,480).isEmpty(),"invalid width");
    for(int height:{0,-1,std::numeric_limits<int>::min()})require(pianoRollOverlayBounds(800,height).isEmpty(),"invalid height");
    const auto huge=pianoRollOverlayBounds(std::numeric_limits<int>::max(),std::numeric_limits<int>::max());
    require(huge.getRight()<=std::numeric_limits<int>::max() && huge.getBottom()<=std::numeric_limits<int>::max(),"layout overflow");
    Session s;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);
    s.tracks[0].notes={{0,200,1,30},{127,300,1,70}};s.addClip(1,{"untouched.wav",0,2,0});
    const auto original=juce::JSON::toString(sessionToJson(s),true);
    IDWPianoRoll editor;editor.session=&s;editor.setSize(1410,480);editor.setTrackIndex(0);
    int changes=0;editor.onChanged=[&]{++changes;};require(editor.selectAdjacentNote(true),"initial selection");
    juce::TextButton* next=nullptr;juce::Label* summary=nullptr;
    for(int i=0;i<editor.getNumChildComponents();++i){auto* c=editor.getChildComponent(i);
        if(auto* b=dynamic_cast<juce::TextButton*>(c))if(b->getButtonText()=="NOTE >")next=b;
        if(c->getComponentID()=="midi-note-summary")summary=dynamic_cast<juce::Label*>(c);
    }
    require(next && summary,"selection controls");
    for(int width:{800,1000,1180,1280,1450,1920})for(int height:{480,600,720,768,850,1080}){
        const auto bounds=pianoRollOverlayBounds(width,height);
        require(juce::Rectangle<int>(0,0,width,height).contains(bounds) && bounds.getHeight()>=240,"overlay outside parent");
        editor.setBounds(bounds);
        require(summary->getBottom()<=editor.getHeight() && next->getY()>=PianoRollViewport::headerHeight+13,"footer outside editor");
        require(summary->getText().contains("Pitch C-1 (0)") && summary->getText().contains("Start 200.000 beats"),"resize lost selection");
        for(int i=0;i<editor.getNumChildComponents();++i){auto* c=editor.getChildComponent(i);
            if(c->isVisible())require(editor.getLocalBounds().contains(c->getBounds()),"visible control clipped");
        }
        // Real painter must still show the selected offscreen-time/low-pitch
        // note after shrinking. Selection reveal changes only the viewport.
        juce::Image image(juce::Image::ARGB,editor.getWidth(),editor.getHeight(),true,juce::SoftwareImageType{});
        {juce::Graphics g(image);editor.paint(g);}
        bool found=false;
        for(int y=PianoRollViewport::headerHeight;y<editor.getHeight()-IDWPianoRoll::velocityFooterHeight && !found;++y)
            for(int x=PianoRollViewport::keyWidth;x<editor.getWidth();++x)if(image.getPixelAt(x,y)==juce::Colours::gold){found=true;break;}
        require(found,"selected note lost outside compact viewport");
    }
    next->onClick();require(summary->getText().contains("Pitch G9 (127)"),"high-pitch navigation after resize");
    require(changes==0 && juce::JSON::toString(sessionToJson(s),true)==original && !editor.restoreHistory(true),"layout changed session/history");
    require(editor.setSelectionVelocity(100) && editor.restoreHistory(true),"history fixture");
    editor.setBounds(pianoRollOverlayBounds(800,480));next->onClick();
    require(editor.restoreHistory(false) && s.tracks[0].notes[1].velocity==100,"resize/navigation destroyed redo");
    Session read;require(sessionFromJson(sessionToJson(s),read) && sameMidiNotes(read.tracks[0].notes,s.tracks[0].notes),"resized notes persistence");
    for(int width:{1,20,100,400})for(int height:{1,20,100,200})
        require(juce::Rectangle<int>(0,0,width,height).contains(pianoRollOverlayBounds(width,height)),"tiny geometry outside parent");
    std::cout<<"Piano roll responsive layout tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
