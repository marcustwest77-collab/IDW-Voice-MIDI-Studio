#include "PreparedPlayback.h"
#include <juce_events/juce_events.h>
#include "PlaybackLoopSettings.h"
#include "ProjectDocumentStatus.h"
#include "OfflineBounce.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto root=temp.getNonexistentChildFile("idw-live-loop","",false);
    try{
        juce::ScopedJuceInitialiser_GUI init;
        Session s;s.bpm=120;s.addTrack(TrackType::midi);s.tracks[0].notes={{60,0,1,30},{64,3,1,90}};
        const auto original=juce::JSON::toString(sessionToJson(s),true);
        auto legacy=sessionToJson(s);legacy.getDynamicObject()->removeProperty("playbackLoop");Session loaded;
        require(sessionFromJson(legacy,loaded) && !loaded.playbackLoopEnabled && loaded.playbackLoop.startBeat==0 && loaded.playbackLoop.endBeat==4,"legacy loop defaults");
        for(const auto& text:juce::StringArray{""," ",".","1.2.3","-1","NaN","inf","1e2","1,5","1000001","1;2"})
            require(!parsePlaybackLoopBeat(text),"invalid loop text");
        require(parsePlaybackLoopBeat(" 0.25 ")==0.25 && parsePlaybackLoopBeat("1000000")==1000000 && parsePlaybackLoopBeat(".5")==0.5,"valid decimal loop text");
        require(validPlaybackLoop({0.1,0.35}) && playbackLoopFrames({0.1,0.35},120,44100).has_value(),"decimal quarter-beat region lost to subtraction rounding");
        const juce::File origin=root.getChildFile("Session.idwproj");
        PlaybackLoopSettingsPlan plan(s,origin);ProjectDocumentStatus status;status.begin(s,origin);
        require(plan.apply(s,origin,false,true,"0","4")==PlaybackLoopSettingsPlan::Result::unavailable && !status.hasUnsavedChanges(s),"busy loop edit");
        require(plan.apply(s,origin,true,true,"4","4")==PlaybackLoopSettingsPlan::Result::invalid && juce::JSON::toString(sessionToJson(s),true)==original,"invalid partial loop edit");
        require(plan.apply(s,origin,true,true,"1.25","4")==PlaybackLoopSettingsPlan::Result::changed && status.hasUnsavedChanges(s),"apply loop settings/dirty state");
        const auto configured=juce::JSON::toString(sessionToJson(s),true);
        require(plan.apply(s,origin,true,false,"0","4")==PlaybackLoopSettingsPlan::Result::unavailable && juce::JSON::toString(sessionToJson(s),true)==configured,"loop edit replay");
        auto unrelated=s;unrelated.playbackLoopEnabled=false;unrelated.playbackLoop={0,4};
        require(juce::JSON::toString(sessionToJson(unrelated),true)==original,"loop edit changed other fields");
        PlaybackLoopSettingsPlan stale(s,origin);s.tracks[0].name="Changed";
        require(stale.apply(s,origin,true,false,"0","4")==PlaybackLoopSettingsPlan::Result::unavailable && s.playbackLoopEnabled,"stale loop dialog applied");
        PlaybackLoopSettingsPlan path(s,origin);
        require(path.apply(s,root.getChildFile("Other.idwproj"),true,false,"0","4")==PlaybackLoopSettingsPlan::Result::unavailable,"changed project identity");
        PlaybackLoopSettingsPlan noop(s,origin);
        require(noop.apply(s,origin,true,true,"1.25","4")==PlaybackLoopSettingsPlan::Result::unchanged,"loop no-op");
        require(root.createDirectory().wasOk() && saveSession(s,origin) && loadSession(origin,loaded) && loaded.playbackLoopEnabled &&
            loaded.playbackLoop.startBeat==1.25 && loaded.playbackLoop.endBeat==4,"loop save/reload");
        status.begin(s,origin);require(!status.hasUnsavedChanges(s),"loop save baseline");
        for(const auto& field:juce::StringArray{"enabled","startBeat","endBeat"}){
            auto bad=sessionToJson(s);bad.getDynamicObject()->getProperty("playbackLoop").getDynamicObject()->removeProperty(field);
            const auto before=juce::JSON::toString(sessionToJson(loaded),true);
            require(!sessionFromJson(bad,loaded) && juce::JSON::toString(sessionToJson(loaded),true)==before,"partial loop block accepted/mutated output");
        }
        for(const juce::var& badValue:std::vector<juce::var>{juce::var("2"),juce::var(true),juce::var(-1.0),juce::var(std::numeric_limits<double>::infinity())}){
            auto bad=sessionToJson(s);bad.getDynamicObject()->getProperty("playbackLoop").getDynamicObject()->setProperty("startBeat",badValue);
            require(!sessionFromJson(bad,loaded),"coerced/invalid loop beat accepted");
        }
        auto badBool=sessionToJson(s);badBool.getDynamicObject()->getProperty("playbackLoop").getDynamicObject()->setProperty("enabled",1);
        require(!sessionFromJson(badBool,loaded),"coerced loop bool accepted");
        Session invalid=s;invalid.playbackLoop.endBeat=invalid.playbackLoop.startBeat;
        juce::MemoryBlock savedBytes;require(origin.loadFileAsData(savedBytes) && !saveSession(invalid,origin),"invalid loop saved");
        juce::MemoryBlock afterBytes;require(origin.loadFileAsData(afterBytes) && savedBytes==afterBytes,"invalid loop replaced saved project");
        auto live=preparePlayback(s,44100,true);require(live && live->loop && live->firstFrame==27563,"prepared loop start frame");
        const auto frozen=live->loop;PlaybackLoopSettingsPlan changed(s,origin);
        require(changed.apply(s,origin,true,true,"0","8")==PlaybackLoopSettingsPlan::Result::changed && live->loop->start==frozen->start && live->loop->end==frozen->end,"active snapshot changed with editable loop");
        auto overdub=preparePlayback(s,44100,false);require(overdub && !overdub->loop && overdub->firstFrame==0 && s.playbackLoopEnabled,"overdub did not isolate loop preference");
        int calls=0;require(renderPreparedPlayback(*live,false,0,512,[&](int,int,std::int64_t){++calls;})==0 && calls==0,"stopped transport rendered/resumed loop");
        std::int64_t cursor=live->loop->end-2;int count=0;
        const auto next=renderPreparedPlayback(*live,true,cursor,512,[&](int offset,int n,std::int64_t frame){
            require(offset==count && frame>=live->loop->start && frame+n<=live->loop->end,"snapshot segment boundary");count+=n;
        });require(next && *next==live->loop->start+510 && count==512,"live snapshot wrap");
        s.playbackLoopEnabled=false;auto linear=preparePlayback(s,44100,true);
        require(linear && !linear->loop && linear->firstFrame==0,"disabled loop linear play");
        require(!preparePlayback(invalid,44100,true),"invalid loop prepared");
        s.playbackLoopEnabled=true;require(!preparePlayback(s,0,true) && preparePlayback(s,0,false).has_value(),"invalid loop device rate / overdub isolation");
        // Export must render the complete arrangement once, irrespective of loop.
        juce::String error;const auto loopWav=root.getChildFile("LoopPreference.wav"),linearWav=root.getChildFile("Linear.wav");
        require(renderProjectWav(s,loopWav,error,8000),"export with loop preference");s.playbackLoopEnabled=false;
        require(renderProjectWav(s,linearWav,error,8000),"linear export");juce::MemoryBlock a,b;
        require(loopWav.loadFileAsData(a) && linearWav.loadFileAsData(b) && a==b,"loop altered/truncated export");
        require(root.getParentDirectory()==temp && root.deleteRecursively(),"cleanup");
        std::cout<<"Live playback loop settings/snapshot tests passed\n";return 0;
    }catch(const std::exception& e){if(root.getParentDirectory()==temp)root.deleteRecursively();std::cerr<<e.what()<<'\n';return 1;}
}
