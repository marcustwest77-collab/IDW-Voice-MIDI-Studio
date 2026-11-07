#include "PianoRoll.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MouseEvent mouse(IDWPianoRoll& e,int x,int y){const auto now=juce::Time::getCurrentTime();
    return {juce::Desktop::getInstance().getMainMouseSource(),{(float)x,(float)y},juce::ModifierKeys::leftButtonModifier,
        1,0,0,0,0,&e,&e,now,{(float)x,(float)y},now,1,false};}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-pointer-snap","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        for(int divisions:{1,2,3,4,6,8}){
            require(snapPianoRollBeats(0,divisions)==0,"zero snap");
            require(snapPianoRollBeats(0.5/divisions,divisions)==1.0/divisions,"positive tie");
            require(snapPianoRollBeats(-0.5/divisions,divisions)==-1.0/divisions,"negative tie");
            require(snapPianoRollBeats(0.49/divisions,divisions)==0,"below tie");
            require(snapPianoRollBeats(-0.49/divisions,divisions)==0,"negative below tie");
            require(!snapPianoRollBeats(std::numeric_limits<double>::quiet_NaN(),divisions) &&
                    !snapPianoRollBeats(std::numeric_limits<double>::infinity(),divisions) && !snapPianoRollBeats(1e308,divisions),"unrepresentable snap");
            Session s;s.addTrack(TrackType::midi);IDWPianoRoll editor;editor.session=&s;editor.setSize(1200,480);editor.setTrackIndex(0);
            require(editor.setSnapDivisions(divisions),"set supported grid");
            editor.mouseDown(mouse(editor,64,340));editor.mouseUp(mouse(editor,64,340));
            require(s.tracks[0].notes.size()==1 && s.tracks[0].notes[0].startBeat==*snapPianoRollBeats(10.0/16,divisions) && s.tracks[0].notes[0].lengthBeats==1,"creation grid");
            require(editor.restoreHistory(true) && s.tracks[0].notes.empty(),"creation undo");
            require(editor.restoreHistory(false) && s.tracks[0].notes.size()==1,"creation redo");
        }
        for(int bad:{-1,0,5,7,9,std::numeric_limits<int>::max()})require(!snapPianoRollBeats(1,bad),"invalid grid");
        Session s;s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);s.tracks[0].notes={{60,1.25,1,30},{64,3,1,40}};
        s.bpm=93;s.addClip(1,{"untouched.wav",0,2,0});const auto original=juce::JSON::toString(sessionToJson(s),true);
        IDWPianoRoll editor;editor.session=&s;editor.setSize(1200,480);editor.setTrackIndex(0);
        bool allowed=true;int changes=0;editor.canEdit=[&]{return allowed;};editor.onChanged=[&]{++changes;};
        juce::TextButton* button=nullptr;juce::Slider* velocity=nullptr;
        for(int i=0;i<editor.getNumChildComponents();++i){auto* c=editor.getChildComponent(i);
            if(auto* b=dynamic_cast<juce::TextButton*>(c))if(b->getButtonText()=="SNAP 1/16")button=b;
            if(auto* v=dynamic_cast<juce::Slider*>(c))velocity=v;
        }
        require(button && velocity && button->isEnabled() && editor.getSnapDivisions()==4,"snap controls/default");
        require(editor.setSnapDivisions(1) && button->getButtonText()=="SNAP 1/4" && changes==0 && juce::JSON::toString(sessionToJson(s),true)==original,"grid setting edited session");
        require(!editor.setSnapDivisions(5) && editor.getSnapDivisions()==1,"invalid grid changed view");
        allowed=false;require(!editor.setSnapDivisions(2),"busy setting");allowed=true;
        // Coarse pointer movement preserves the original off-grid offset.
        editor.mouseDown(mouse(editor,78,340));require(!editor.setSnapDivisions(8),"grid changed during note drag");
        editor.mouseDrag(mouse(editor,86,340));editor.mouseUp(mouse(editor,86,340));
        require(s.tracks[0].notes[0].startBeat==2.25 && s.tracks[0].notes[0].velocity==30,"coarse move");
        require(editor.restoreHistory(true),"move undo");require(editor.setSnapDivisions(8),"finer grid");
        require(editor.restoreHistory(false) && s.tracks[0].notes[0].startBeat==2.25,"view setting destroyed redo");
        require(editor.restoreHistory(true),"restore move");
        // A 1/32 grid supports an eighth-beat minimum length on resize.
        editor.mouseDown(mouse(editor,88,340));editor.mouseDrag(mouse(editor,74,340));editor.mouseUp(mouse(editor,74,340));
        require(s.tracks[0].notes[0].lengthBeats==0.125 && s.tracks[0].notes[0].startBeat==1.25,"fine resize minimum");
        require(editor.restoreHistory(true) && s.tracks[0].notes[0].lengthBeats==1,"fine resize undo");
        editor.mouseDown(mouse(editor,78,340));editor.mouseUp(mouse(editor,78,340));
        velocity->onDragStart();require(!editor.setSnapDivisions(1),"grid changed during velocity gesture");velocity->onDragEnd();
        require(juce::JSON::toString(sessionToJson(s),true)==original,"snap altered unrelated data");
        require(root.createDirectory().wasOk(),"fixture folder");const auto file=root.getChildFile("Snap.idwproj");
        require(saveSession(s,file),"save snap view");Session read;
        require(loadSession(file,read) && juce::JSON::toString(sessionToJson(read),true)==original,"snap view serialized into project");
        editor.setSize(1000,480);require(!button->isVisible(),"narrow snap overlaps footer");editor.setSize(1200,480);
        require(button->isVisible() && velocity->getRight()<=button->getX(),"snap overlaps inspector");
        editor.setTrackIndex(1);require(!button->isEnabled() && !editor.setSnapDivisions(4),"audio grid change");
        editor.setTrackIndex(999);require(!editor.setSnapDivisions(4),"invalid grid target");editor.session=nullptr;require(!editor.setSnapDivisions(4),"null grid target");
        editor.session=&s;editor.setTrackIndex(0);editor.resetEditor();require(editor.getSnapDivisions()==4 && button->getButtonText()=="SNAP 1/16","reset grid default");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"Piano roll snap tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
