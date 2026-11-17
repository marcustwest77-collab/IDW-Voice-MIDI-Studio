#include "ClipPositionPlan.h"
#include "ProjectDocumentStatus.h"
#include <iostream>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::String json(const Session& s){return juce::JSON::toString(sessionToJson(s),true);}
int main(){
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-clip-position","",false);
    try{
        require(root.createDirectory().wasOk(),"temp directory");
        const auto source=root.getChildFile("Original.wav");require(source.replaceWithText("unchanged source bytes"),"source fixture");
        juce::MemoryBlock originalBytes;require(source.loadFileAsData(originalBytes),"read source");
        Session original;original.bpm=120;original.addTrack(TrackType::audio);original.addTrack(TrackType::midi);
        require(original.addClip(0,{source.getFullPathName().toStdString(),2,8,0.5}),"clip fixture");
        require(original.setClipFades(0,0,1,1,FadeCurve::linear,FadeCurve::equalPower),"fades");
        const auto file=root.getChildFile("Session.idwproj");juce::String error;
        using Op=ClipPositionPlan::Operation;using Result=ClipPositionPlan::Result;
        for(const auto operation:{Op::move,Op::trimLeft,Op::trimRight}){
            auto s=original;ClipEditHistory history;ProjectDocumentStatus status;status.begin(s,file);
            ClipPositionPlan plan(s,file,0,0);
            for(const auto& bad:juce::StringArray{"",".","-1","nan","inf","1e2","1.2.3","1000001"})
                require(plan.commit(s,history,file,true,operation,bad,error)==Result::rejected && json(s)==json(original) && !history.canUndo(),"invalid mutation");
            require(plan.commit(s,history,file,false,operation,"4",error)==Result::rejected,"busy accepted");
            require(plan.commit(s,history,root.getChildFile("Other.idwproj"),true,operation,"4",error)==Result::rejected,"origin accepted");
            const auto target=operation==Op::trimRight?"7":"4";
            require(plan.commit(s,history,file,true,operation,target,error)==Result::applied && status.hasUnsavedChanges(s),"apply/dirty");
            const auto& c=s.tracks[0].clips[0];
            require(c.startBeat==(operation==Op::trimRight?2:4) && c.lengthBeats==(operation==Op::move?8:operation==Op::trimLeft?6:5) &&
                c.sourceOffsetSeconds==(operation==Op::trimLeft?1.5:0.5) && c.sourcePath==original.tracks[0].clips[0].sourcePath,"timing/source offset");
            require(c.fadeInBeats==1 && c.fadeOutBeats==1 && c.gainDb==0 && s.tracks[1].notes.empty(),"unrelated metadata");
            const auto edited=s;
            require(plan.commit(s,history,file,true,operation,target,error)==Result::rejected,"replay accepted");
            require(history.undo(s)==ClipEditHistory::Result::applied && json(s)==json(original) && !status.hasUnsavedChanges(s),"one Undo/baseline");
            ClipPositionPlan noop(s,file,0,0);
            require(noop.commit(s,history,file,true,operation,operation==Op::trimRight?"10":"2",error)==Result::unchanged && history.canRedo(),"no-op cleared redo");
            ClipPositionPlan invalid(s,file,0,0);
            if(operation!=Op::move)require(invalid.commit(s,history,file,true,operation,operation==Op::trimRight?"11":"10",error)==Result::rejected && history.canRedo(),"invalid trim cleared redo");
            require(history.redo(s)==ClipEditHistory::Result::applied && json(s)==json(edited),"one Redo");
            require(saveSession(s,file),"save");Session loaded;require(loadSession(file,loaded) && json(loaded)==json(s),"reload");
            ClipPositionPlan stale(s,file,0,0);s.masterMix.ceilingDb=-6.5;
            require(stale.commit(s,history,file,true,operation,target,error)==Result::rejected,"stale accepted");
            require(history.undo(s)==ClipEditHistory::Result::applied && s.masterMix.ceilingDb==-6.5,"Undo changed master");
        }
        auto s=original;ClipEditHistory history;
        for(auto op:{Op::trimLeft,Op::trimRight}){
            ClipPositionPlan boundary(s,file,0,0);
            require(boundary.commit(s,history,file,true,op,op==Op::trimLeft?"1":"2",error)==Result::rejected && json(s)==json(original),"extension/empty accepted");
        }
        ClipPositionPlan midi(s,file,1,0),missing(s,file,4,0);
        require(midi.commit(s,history,file,true,Op::move,"4",error)==Result::rejected && missing.commit(s,history,file,true,Op::move,"4",error)==Result::rejected,"bad target");
        ClipPositionPlan shortTrim(s,file,0,0);
        require(shortTrim.commit(s,history,file,true,Op::trimRight,"2.5",error)==Result::applied &&
            s.tracks[0].clips[0].fadeInBeats==0.5 && s.tracks[0].clips[0].fadeOutBeats==0.5,"fade clamp");
        juce::MemoryBlock after;require(source.loadFileAsData(after) && after==originalBytes,"source modified");
        require(root.deleteRecursively(),"cleanup");std::cout<<"Clip position dialog tests passed\n";return 0;
    }catch(const std::exception& e){root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
