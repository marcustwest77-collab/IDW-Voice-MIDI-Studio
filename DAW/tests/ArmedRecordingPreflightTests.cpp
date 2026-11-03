#include "RecordingPreflight.h"
#include "SessionPersistence.h"
#include "LoopbackCapture.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static ArmedRecordingTargets inspect(const Session& s,const std::array<bool,128>& armed,
        const juce::StringArray& names,const juce::BigInteger& active,size_t capacity=8){
    const auto before=juce::JSON::toString(sessionToJson(s));const auto flags=armed;
    auto result=inspectArmedRecordingTargets(s,armed,names,active,capacity);
    require(before==juce::JSON::toString(sessionToJson(s)) && flags==armed,"inspection mutated session or arm flags");
    for(const auto& target:result.tracks)if(target.routeValid)
        require(target.callbackInput==IDWLoopbackCapture::callbackIndexForChannel(active,target.physicalInput),"callback map differs from recording path");
    return result;
}
int main(){try{
    Session s;s.addTrack(TrackType::audio);s.addTrack(TrackType::midi);s.addTrack(TrackType::audio);
    s.tracks[0].name="Lead";s.tracks[2].name="Harmony";s.tracks[0].inputChannel=0;s.tracks[2].inputChannel=1;
    std::array<bool,128> armed{};armed[0]=true;armed[2]=true;
    const juce::StringArray names{"Mic 1","Mic 2"};juce::BigInteger active;active.setRange(0,2,true);
    const auto live=assessRecordingDevice(true,true,48000,48000,true);
    const auto pass=[&](const ArmedRecordingTargets& targets){return recordingPreflightForTargets(targets,live,true,100LL*1024*1024).passes();};
    const auto valid=inspect(s,armed,names,active);
    require(valid.ready() && valid.tracks.size()==2 && valid.tracks[0].track==0 && valid.tracks[1].track==2 &&
        valid.tracks[0].callbackInput==0 && valid.tracks[1].callbackInput==1 && pass(valid),"valid all-armed destinations rejected");
    // The selected MIDI/unarmed track is deliberately not an input to inspection.
    auto occupied=s;require(occupied.addClip(2,{"original-take.wav",0,1,0}),"occupied fixture");
    const auto occupiedResult=inspect(occupied,armed,names,active);
    require(!occupiedResult.allEmpty && !occupiedResult.ready() && !pass(occupiedResult) &&
        describeArmedRecordingTargets(occupiedResult).joinIntoString("\n").contains("Empty: FAIL"),"second armed occupied track falsely passed");
    auto duplicate=s;duplicate.tracks[2].inputChannel=0;const auto reused=inspect(duplicate,armed,names,active);
    require(!reused.distinctInputs && !reused.tracks[0].uniqueInput && !reused.tracks[1].uniqueInput && !pass(reused),"reused physical input falsely passed");
    require(describeArmedRecordingTargets(reused)[0].contains("DUPLICATE") && describeArmedRecordingTargets(reused)[1].contains("DUPLICATE"),"duplicate report missed affected track");
    juce::BigInteger disabled;disabled.setBit(0);const auto inactive=inspect(s,armed,names,disabled);
    require(!inactive.routesValid && inactive.tracks[1].callbackInput==-1 && !pass(inactive),"disabled second input falsely passed");
    for(int channel:{-1,2,std::numeric_limits<int>::max()}){
        auto bad=s;bad.tracks[2].inputChannel=channel;auto mask=active;if(channel==2)mask.setBit(2);
        const auto result=inspect(bad,armed,names,mask);
        require(!result.routesValid && !result.tracks[1].routeValid && result.tracks[1].callbackInput==-1 && !pass(result),"out-of-range input falsely passed");
    }
    auto sparse=s;sparse.tracks[0].inputChannel=1;sparse.tracks[2].inputChannel=7;
    juce::StringArray eightNames;for(int i=0;i<8;++i)eightNames.add("Physical "+juce::String(i+1));
    juce::BigInteger sparseMask;sparseMask.setBit(1);sparseMask.setBit(7);
    const auto compact=inspect(sparse,armed,eightNames,sparseMask);
    require(pass(compact) && compact.tracks[0].callbackInput==0 && compact.tracks[1].callbackInput==1 &&
        compact.tracks[1].inputName=="Physical 8","sparse physical/callback mapping");
    std::array<bool,128> none{};const auto empty=inspect(s,none,names,active);
    require(!empty.ready() && !pass(empty) && describeArmedRecordingTargets(empty)[0].contains("arm at least one"),"no armed targets falsely passed");
    auto midiOnly=none;midiOnly[1]=true;require(!pass(inspect(s,midiOnly,names,active)),"armed MIDI flag treated as audio destination");
    auto one=none;one[0]=true;one[127]=true;
    require(pass(inspect(occupied,one,names,active)),"unarmed occupied track or stale beyond-session arm flag affected valid destination");
    Session many;juce::StringArray manyNames;juce::BigInteger manyMask;auto manyArmed=none;
    for(int i=0;i<9;++i){many.addTrack(TrackType::audio);many.tracks[(size_t)i].inputChannel=i;manyNames.add("Input "+juce::String(i+1));manyMask.setBit(i);manyArmed[(size_t)i]=true;}
    const auto tooMany=inspect(many,manyArmed,manyNames,manyMask);
    require(!tooMany.withinCapacity && !tooMany.ready() && !pass(tooMany),"over-capacity arm set falsely passed");
    const auto bounded=describeArmedRecordingTargets(tooMany,4);
    require(bounded.size()==5 && bounded[4].contains("5 additional"),"large arm-set report not bounded");
    manyArmed[8]=false;require(pass(inspect(many,manyArmed,manyNames,manyMask)),"eight configured routes rejected");
    require(!pass(inspect(many,manyArmed,manyNames,manyMask,0)),"zero capacity accepted targets");
    auto noisy=s;noisy.tracks[0].name="Lead\r\n\t"+std::string(300,'x');
    const auto line=describeArmedRecordingTargets(inspect(noisy,armed,names,active))[0];
    require(!line.containsAnyOf("\r\n\t") && line.length()<220,"track report unbounded or multiline");
    require(!recordingPreflightForTargets(valid,assessRecordingDevice(true,false,48000,48000,true),true,100LL*1024*1024).passes(),"valid arm set hid closed device");
    require(!recordingPreflightForTargets(valid,assessRecordingDevice(true,true,48000,44100,true),true,100LL*1024*1024).passes(),"valid arm set hid stale rate");
    require(!recordingPreflightForTargets(valid,live,false,100LL*1024*1024).passes() &&
        !recordingPreflightForTargets(valid,live,true,100LL*1024*1024-1).passes(),"valid arm set hid folder or capacity failure");
    std::cout<<"PASS all-armed preflight: independent track scope, occupied/duplicate/disabled routes, sparse callback map, eight-track limit, bounded diagnostics and unchanged session/arm state; no real drivers\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
