#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& e,int x,int y,int mods=0){
    const auto now=juce::Time::getCurrentTime();y+=PianoRollViewport::headerHeight;
    return {juce::Desktop::getInstance().getMainMouseSource(),{(float)x,(float)y},
            juce::ModifierKeys(mods),1,0,0,0,0,&e,&e,now,{(float)x,(float)y},now,1,false};
}
static void wheel(IDWPianoRoll& e,float x,float y,int mods=0,int cursorX=250){
    juce::MouseWheelDetails details{};details.deltaX=x;details.deltaY=y;
    e.mouseWheelMove(mouse(e,cursorX,10,mods),details);
}
int main(){try{
    juce::ScopedJuceInitialiser_GUI init;
    PianoRollViewport view;
    // All 128 MIDI pitches remain reachable at multiple editor heights.
    for(int height:{130,456,1600}){
        view.scrollPitch(10000,height);require(view.topPitch==127,"upper pitch clamp");
        require(view.pitchAt(0,height)==127,"highest pitch inaccessible");
        view.scrollPitch(-10000,height);
        const int bottom=(PianoRollViewport::rows(height)-1)*13;
        require(view.pitchAt(bottom,height)==0,"lowest pitch inaccessible");
        require(!view.pitchAt(-1,height) && !view.pitchAt(bottom+13,height),"grid boundary accepted");
    }
    view={};view.scrollTime(100,600,456,300);
    const double anchor=view.beatAt(240);
    view.zoomAt(1,240,600,456,300);
    require(std::abs(view.beatAt(240)-anchor)<1e-12,"zoom lost cursor beat");
    require(view.pixelsPerBeat()==24,"zoom scale");
    view.scrollTime(-10000,600,456,300);require(view.firstBeat==0,"negative time scroll");
    view.zoomAt(-100,0,600,456,300);require(view.pixelsPerBeat()==8,"minimum zoom");
    view.zoomAt(100,0,600,456,300);require(view.pixelsPerBeat()==48,"maximum zoom");
    view.firstBeat=100;view.zoomLevel=1;view.topPitch=84;
    auto spanning=view.bounds({60,0,1000,100},600,456);
    require(spanning && spanning->x==0 && spanning->width==600 && !spanning->rightEdgeVisible,
            "spanning note not clipped or false resize edge");
    require(!view.bounds({60,1000,1,100},600,456),"offscreen note visible");
    require(!view.bounds({60,1e300,1e300,100},600,456),"huge note narrowed to pixels");
    require(!view.bounds({60,0,std::numeric_limits<double>::infinity(),100},600,456),"nonfinite bounds");
    view.firstBeat=std::numeric_limits<double>::quiet_NaN();view.constrain(600,456,300);
    require(view.firstBeat==0,"nonfinite view survived");

    Session s;s.addTrack(TrackType::midi);require(s.addMidiNote(0,{60,200,4,100}),"long song fixture");
    IDWPianoRoll editor;editor.session=&s;editor.setSize(1000,480);
    const auto original=s.tracks[0].notes;
    wheel(editor,0,100);wheel(editor,0,-10,juce::ModifierKeys::shiftModifier);
    require(sameMidiNotes(original,s.tracks[0].notes) && !editor.restoreHistory(true),"navigation edited notes/history");
    editor.mouseDown(mouse(editor,56,1));editor.mouseUp(mouse(editor,56,1));
    require(s.tracks[0].notes.back().pitch==127 && s.tracks[0].notes.back().startBeat==149,"scrolled draw mapping");
    // Anchor zoom at beat 161.125, then draw at a different pitch.
    wheel(editor,0,1,juce::ModifierKeys::commandModifier);
    editor.mouseDown(mouse(editor,250,14));editor.mouseUp(mouse(editor,250,14));
    require(s.tracks[0].notes.back().pitch==126 && s.tracks[0].notes.back().startBeat==161.25,"zoomed draw mapping");
    editor.mouseDown(mouse(editor,273,14));
    // Navigation during a drag cannot change its coordinate system.
    wheel(editor,0,-100);wheel(editor,0,-1,juce::ModifierKeys::commandModifier);
    editor.mouseDrag(mouse(editor,297,14));editor.mouseUp(mouse(editor,297,14));
    require(s.tracks[0].notes.back().lengthBeats==2,"zoomed resize distance");
    require(editor.restoreHistory(true) && s.tracks[0].notes.back().lengthBeats==1,"navigation entered undo history");
    require(editor.restoreHistory(false) && s.tracks[0].notes.back().lengthBeats==2,"zoomed redo");
    wheel(editor,0,-100);
    const int bottomRow=PianoRollViewport::rows(480-PianoRollViewport::headerHeight-IDWPianoRoll::velocityFooterHeight)-1;
    editor.mouseDown(mouse(editor,400,bottomRow*13+1));editor.mouseUp(mouse(editor,400,bottomRow*13+1));
    require(s.tracks[0].notes.back().pitch==0,"lowest pitch draw mapping");
    // Shrinking the viewport and scrolling to earlier time keeps notes unchanged.
    const auto edited=s.tracks[0].notes;
    editor.setSize(800,350);wheel(editor,10,0);
    require(sameMidiNotes(edited,s.tracks[0].notes),"resize or horizontal wheel mutated notes");
    Session reopened;require(sessionFromJson(sessionToJson(s),reopened) &&
        sameMidiNotes(edited,reopened.tracks[0].notes),"navigation affected persisted timing");
    // Exercise the real painter with offscreen and enormous finite positions.
    Session extreme;extreme.addTrack(TrackType::midi);
    extreme.tracks[0].notes={{72,0,1000,100},{60,1e300,1e300,100},{127,0,1,100}};
    editor.session=&extreme;editor.resetEditor();
    juce::Image image(juce::Image::ARGB,800,350,true,juce::SoftwareImageType{});
    {juce::Graphics graphics(image);editor.paint(graphics);}
    require(image.getPixelAt(100,PianoRollViewport::headerHeight+12*13+4)==juce::Colours::orange,
            "clipped spanning note painter disagrees with hit geometry");
    std::cout<<"PASS piano-roll full pitch range, time scroll, anchored zoom, clipped bounds, actual draw/resize mapping, undo isolation and persistence\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
