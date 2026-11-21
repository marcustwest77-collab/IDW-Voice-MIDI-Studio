#include "ClipSplitPlan.h"
#include "StudioControlLayout.h"
#include "OfflineBounce.h"
#include "ProjectDocumentStatus.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::String json(const Session& s){return juce::JSON::toString(sessionToJson(s),true);}
static juce::MemoryBlock bytes(const juce::File& file){juce::MemoryBlock b;require(file.loadFileAsData(b),"read bytes");return b;}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-clip-split-dialog","",false);
    try{
        for(int width:{1240,1280,1450,1920,8192}){
            const auto commands=studioCommandBounds(width);
            const juce::Rectangle<int> available(620,250,width-640,72);
            for(size_t i=0;i<commands.size();++i){
                require(available.contains(commands[i]) && commands[i].getWidth()>=79 && commands[i].getHeight()>=22,
                        "command control clipped or too small");
                for(size_t j=i+1;j<commands.size();++j)
                    require(!commands[i].intersects(commands[j]),"command controls overlap");
            }
        }
        for(const auto& bounds:studioCommandBounds(800))require(bounds.isEmpty(),"unsupported command width");
        for(int width:{800,1280,1450,1920,std::numeric_limits<int>::max()}){
            const auto rows=studioControlRows(width);
            const juce::Rectangle<int> ruler(20,330,width-40,36);
            for(size_t i=0;i<rows.size();++i){
                require(rows[i].getY()>=62 && rows[i].getBottom()<=322 && rows[i].getHeight()>=20 &&
                    !rows[i].intersects(ruler),"controls intercept the ruler");
                for(size_t j=i+1;j<rows.size();++j)require(!rows[i].intersects(rows[j]),"control rows overlap");
            }
            require(rows[synthRow].getBottom()<330 && rows[eqRow].getBottom()<330 &&
                rows[ceilingRow].getBottom()<330 && rows[statusRow].getHeight()>=20,"processing/status controls overlap timeline");
        }
        for(int width:{0,-1,40,std::numeric_limits<int>::min()})
            for(const auto& row:studioControlRows(width))require(row.isEmpty(),"invalid width geometry");
        require(root.createDirectory().wasOk(),"temp folder");
        const auto wavFile=root.getChildFile("Original.wav");
        {
            auto stream=wavFile.createOutputStream();require(stream!=nullptr,"source stream");juce::WavAudioFormat format;
            auto writer=std::unique_ptr<juce::AudioFormatWriter>(format.createWriterFor(stream.get(),8000,2,24,{},0));
            require(writer!=nullptr,"source writer");stream.release();
            juce::AudioBuffer<float> samples(2,16000);
            for(int i=0;i<16000;++i){const float value=0.2f*std::sin((float)i*0.1f);samples.setSample(0,i,value);samples.setSample(1,i,-value);}
            require(writer->writeFromFloatArrays(samples.getArrayOfReadPointers(),2,16000),"source samples");
        }
        const auto sourceBytes=bytes(wavFile);
        Session s;s.bpm=120;s.addTrack(TrackType::audio);
        require(s.addClip(0,{wavFile.getFullPathName().toStdString(),1,4,0}),"fixture clip");
        require(s.setClipFades(0,0,0.25,0.25,FadeCurve::linear,FadeCurve::equalPower),"fixture fades");
        const auto original=s;const auto file=root.getChildFile("Source.idwproj");
        ProjectDocumentStatus status;status.begin(s,file);ClipEditHistory history;juce::String error;
        ClipSplitPlan plan(s,file,0,0);
        for(const auto& input:juce::StringArray{""," ",".","NaN","inf","1e2","-1","1.2.3","1000001","1","5","1.1"})
            require(!plan.commit(s,history,file,true,input,error) && json(s)==json(original) && !history.canUndo(),"invalid split mutated work/history");
        require(!plan.commit(s,history,file,false,"3",error) && json(s)==json(original),"busy split applied");
        require(plan.commit(s,history,file,true," 3.0 ",error) && status.hasUnsavedChanges(s),"valid split/dirty");
        require(s.tracks[0].clips.size()==2 && s.tracks[0].clips[0].lengthBeats==2 &&
            s.tracks[0].clips[1].startBeat==3 && s.tracks[0].clips[1].lengthBeats==2 &&
            s.tracks[0].clips[1].sourceOffsetSeconds==1 && s.tracks[0].clips[0].fadeOutBeats==0 &&
            s.tracks[0].clips[1].fadeInBeats==0,"split timing/source offsets/fades");
        require(!plan.commit(s,history,file,true,"4",error),"split request replay");
        const auto split=s;
        require(history.undo(s)==ClipEditHistory::Result::applied && json(s)==json(original) &&
            !status.hasUnsavedChanges(s) && !history.canUndo() && history.canRedo(),"one undo restores clip/baseline");
        ClipSplitPlan invalid(s,file,0,0);
        require(!invalid.commit(s,history,file,true,"1",error) && history.canRedo(),"invalid dialog cleared redo");
        require(history.redo(s)==ClipEditHistory::Result::applied && json(s)==json(split) && !history.canRedo(),"one redo restores split");
        require(saveSession(s,root.getChildFile("Split.idwproj")),"save split");Session loaded;
        require(loadSession(root.getChildFile("Split.idwproj"),loaded) && json(loaded)==json(s),"reload split");
        const auto beforeWav=root.getChildFile("Before.wav"),afterWav=root.getChildFile("After.wav");
        require(renderProjectWav(original,beforeWav,error,8000) && renderProjectWav(s,afterWav,error,8000) &&
            bytes(beforeWav)==bytes(afterWav) && bytes(wavFile)==sourceBytes,"split altered mix/source bytes");
        ClipSplitPlan stale(s,file,0,0);s.masterMix.ceilingDb=-6.5;
        require(!stale.commit(s,history,file,true,"2",error) && s.masterMix.ceilingDb==-6.5,"stale dialog overwrote work");
        ClipSplitPlan wrongPath(s,file,0,0);
        require(!wrongPath.commit(s,history,root.getChildFile("Other.idwproj"),true,"2",error),"changed project origin accepted");
        require(history.undo(s)==ClipEditHistory::Result::applied && s.masterMix.ceilingDb==-6.5,"audio Undo restored unrelated master state");
        Session midi;midi.addTrack(TrackType::midi);ClipSplitPlan badTarget(midi,file,0,0);ClipEditHistory empty;
        require(!badTarget.commit(midi,empty,file,true,"2",error),"MIDI target accepted");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"Clip split dialog and control-layout tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
