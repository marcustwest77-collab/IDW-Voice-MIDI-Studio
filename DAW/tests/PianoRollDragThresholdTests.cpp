#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& e,int x,int y,int downX,int downY){
    const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{(float)x,(float)y},
        juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&e,&e,now,{(float)downX,(float)downY},now,1,true};
}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-drag-threshold","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        Session s;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);
        s.tracks[0].notes={{60,1.25,1,30},{64,3,1,40}};s.bpm=93;s.masterMix.gain=0.8;
        s.addClip(1,{"untouched.wav",0,2,0});
        const auto baseline=s.tracks[0].notes;
        const auto original=juce::JSON::toString(sessionToJson(s),true);
        IDWPianoRoll editor;editor.session=&s;editor.setSize(1200,480);editor.setTrackIndex(0);
        bool allowed=true;editor.canEdit=[&]{return allowed;};int changes=0;editor.onChanged=[&]{++changes;};
        juce::Slider* velocity=nullptr;
        for(int i=0;i<editor.getNumChildComponents();++i)if(auto* v=dynamic_cast<juce::Slider*>(editor.getChildComponent(i)))velocity=v;
        require(velocity!=nullptr,"velocity inspector missing");
        // First note occupies x74..90, y336..349 at default zoom. The body
        // starts a move; the last six pixels start a resize.
        constexpr int bodyX=78,edgeX=88,y=340;
        for(int x:{bodyX,edgeX})for(int dx:{-3,-2,-1,0,1,2,3})for(int dy:{-3,0,3}){
            editor.mouseDown(mouse(editor,x,y,x,y));
            editor.mouseDrag(mouse(editor,x+dx,y+dy,x,y));
            editor.mouseUp(mouse(editor,x+dx,y+dy,x,y));
            require(sameMidiNotes(s.tracks[0].notes,baseline) && changes==0,"selection jitter changed note");
            require(velocity->isEnabled() && velocity->getValue()==30,"selection jitter lost inspector");
        }
        require(!editor.restoreHistory(true),"selection jitter created undo step");
        require(editor.adjustTrackVelocity(MidiTrackVelocityMode::offset,10),"velocity edit");
        require(editor.restoreHistory(true),"undo velocity");
        editor.mouseDown(mouse(editor,bodyX,y,bodyX,y));
        editor.mouseDrag(mouse(editor,bodyX+2,y,bodyX,y));editor.mouseUp(mouse(editor,bodyX+2,y,bodyX,y));
        require(editor.restoreHistory(false) && s.tracks[0].notes[0].velocity==40 && s.tracks[0].notes[1].velocity==50,"selection jitter destroyed redo");
        require(editor.restoreHistory(true) && sameMidiNotes(s.tracks[0].notes,baseline),"restore original");
        // At four pixels the intended quarter-beat move must still work.
        editor.mouseDown(mouse(editor,bodyX,y,bodyX,y));
        editor.mouseDrag(mouse(editor,bodyX+4,y,bodyX,y));
        require(s.tracks[0].notes[0].startBeat==1.5,"threshold blocked intentional move");
        editor.mouseDrag(mouse(editor,bodyX+1,y,bodyX,y));
        require(s.tracks[0].notes[0].startBeat==1.25,"latched drag could not return near origin");
        editor.mouseDrag(mouse(editor,bodyX+8,y-13,bodyX,y));
        editor.mouseUp(mouse(editor,bodyX+8,y-13,bodyX,y));
        require(s.tracks[0].notes[0].startBeat==1.75 && s.tracks[0].notes[0].pitch==61,"intentional move/pitch");
        require(editor.restoreHistory(true) && sameMidiNotes(s.tracks[0].notes,baseline),"drag not grouped");
        require(editor.restoreHistory(false) && s.tracks[0].notes[0].pitch==61,"drag redo");
        require(editor.restoreHistory(true),"restore after drag");
        // Resizing also needs the threshold and remains one history step.
        editor.mouseDown(mouse(editor,edgeX,y,edgeX,y));
        editor.mouseDrag(mouse(editor,edgeX+4,y,edgeX,y));editor.mouseUp(mouse(editor,edgeX+4,y,edgeX,y));
        require(s.tracks[0].notes[0].lengthBeats==1.25 && s.tracks[0].notes[0].startBeat==1.25,"intentional resize");
        require(editor.restoreHistory(true) && sameMidiNotes(s.tracks[0].notes,baseline),"resize undo");
        // A fresh gesture must not inherit the previous drag's latched state.
        editor.mouseDown(mouse(editor,edgeX,y,edgeX,y));
        editor.mouseDrag(mouse(editor,edgeX+2,y,edgeX,y));editor.mouseUp(mouse(editor,edgeX+2,y,edgeX,y));
        require(sameMidiNotes(s.tracks[0].notes,baseline) && editor.restoreHistory(false),"new click inherited old drag latch");
        require(editor.restoreHistory(true),"restore resize");
        editor.mouseDown(mouse(editor,bodyX,y,bodyX,y));allowed=false;
        editor.mouseDrag(mouse(editor,bodyX+8,y,bodyX,y));editor.mouseUp(mouse(editor,bodyX+8,y,bodyX,y));allowed=true;
        require(sameMidiNotes(s.tracks[0].notes,baseline),"busy drag changed notes");
        require(juce::JSON::toString(sessionToJson(s),true)==original,"other session data changed");
        require(root.createDirectory().wasOk(),"fixture directory");const auto file=root.getChildFile("Selected.idwproj");
        require(saveSession(s,file),"save selected notes");Session read;
        require(loadSession(file,read) && sameMidiNotes(read.tracks[0].notes,baseline),"jitter persisted timing change");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"Piano roll drag threshold tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
