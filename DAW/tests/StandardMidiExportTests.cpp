#include "StandardMidiExport.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::MemoryBlock bytes(const juce::File& f){juce::MemoryBlock b;require(f.loadFileAsData(b),"read file bytes");return b;}
static juce::MidiFile read(const juce::File& f){
    juce::FileInputStream input(f);juce::MidiFile midi;int type=-1;
    require(midi.readFrom(input,false,&type) && type==1 && midi.getTimeFormat()==960,"SMF read/type/division");return midi;
}
// Independently decode SMF track chunks, VLQ deltas and running status instead
// of relying solely on the same JUCE reader used by the production verifier.
static void verifyBinaryEvents(const juce::MemoryBlock& data){
    const auto* p=(const unsigned char*)data.getData();const size_t size=data.getSize();size_t at=14;
    std::vector<std::vector<int>> notes;
    bool tempo=false,meter=false;
    for(int track=0;track<3;++track){
        require(at+8<=size && std::memcmp(p+at,"MTrk",4)==0,"SMF track chunk");at+=4;
        const size_t count=((size_t)p[at]<<24)|((size_t)p[at+1]<<16)|((size_t)p[at+2]<<8)|p[at+3];at+=4;
        require(count<=size-at,"truncated track chunk");const auto end=at+count;
        const auto vlq=[&](){int value=0;
            for(int i=0;i<4;++i){require(at<end,"truncated VLQ");const auto b=p[at++];value=(value<<7)|(b&127);if(!(b&128))return value;}
            throw std::runtime_error("invalid VLQ length");};
        int tick=0,running=0;bool ended=false;
        while(at<end){
            tick+=vlq();require(at<end,"missing event status");int status=p[at];
            if(status>=128){++at;if(status<240)running=status;else running=0;}
            else {require(running!=0,"running status without channel message");status=running;}
            if(status==255){
                require(at<end,"missing meta type");const auto type=p[at++];const auto length=(size_t)vlq();
                require(length<=end-at,"truncated meta event");
                if(type==81){require(track==0 && length==3 && tick==0 &&
                    (((int)p[at]<<16)|((int)p[at+1]<<8)|p[at+2])==436364,"binary tempo");tempo=true;}
                if(type==88){require(track==0 && length==4 && tick==0 && p[at]==7 && p[at+1]==3,"binary meter");meter=true;}
                if(type==47){require(length==0 && at==end,"end-of-track placement");ended=true;}
                at+=length;
            }else{
                require(((status&240)==128 || (status&240)==144) && at+2<=end,"unexpected MIDI channel event");
                const int pitch=p[at++],velocity=p[at++];require(pitch<128 && velocity<128,"invalid MIDI data byte");
                notes.push_back({track,tick,status,pitch,velocity});
            }
        }
        require(ended,"missing end-of-track");
    }
    require(at==size && tempo && meter,"binary SMF extent or metadata");
    require(notes==std::vector<std::vector<int>>{{1,118,0x90,60,64},{1,960,0x80,60,0},
        {1,960,0x90,60,99},{1,1440,0x80,60,0},{2,1920,0x91,36,127},{2,2160,0x81,36,0}},
        "independently decoded note event sequence");
}
int main(){try{
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("idw-standard-midi","",false);
    require(root.createDirectory().wasOk(),"fixture directory");
    Session s;s.bpm=137.5;s.numerator=7;s.denominator=8;
    s.addTrack(TrackType::audio);s.addTrack(TrackType::midi);s.addTrack(TrackType::midi);s.addTrack(TrackType::midi);
    s.tracks[1].name=u8"Piano \u03b2";s.tracks[2].name="Bass";s.tracks[3].mute=true;
    require(s.addMidiNote(1,{60,0.1234,0.8766,64}) && s.addMidiNote(1,{60,1,0.5,99}) &&
            s.addMidiNote(2,{36,2,0.25,127}) && s.addMidiNote(3,{72,0,1,100}),"note fixture");
    const auto before=juce::JSON::toString(sessionToJson(s));
    const auto output=root.getChildFile("arrangement.mid");juce::String error="stale";std::atomic<float> progress{0};
    require(exportSessionMidi(s,output,error,&progress) && error.isEmpty() && progress.load()==1,"SMF export");
    const auto raw=bytes(output);require(raw.getSize()>14,"SMF header size");
    const auto* p=(const unsigned char*)raw.getData();
    require(std::memcmp(p,"MThd",4)==0 && p[7]==6 && p[8]==0 && p[9]==1 &&
            p[10]==0 && p[11]==3 && p[12]==3 && p[13]==192,"independent binary SMF header");
    verifyBinaryEvents(raw);
    const auto midi=read(output);require(midi.getNumTracks()==3,"audible MIDI tracks/conductor");
    bool tempo=false,meter=false;
    for(int i=0;i<midi.getTrack(0)->getNumEvents();++i){
        const auto& m=midi.getTrack(0)->getEventPointer(i)->message;
        if(m.isTempoMetaEvent()){
            require(std::abs(m.getTempoSecondsPerQuarterNote()-std::llround(60000000.0/s.bpm)*0.000001)<1e-12,"tempo precision");tempo=true;
        }
        if(m.isTimeSignatureMetaEvent()){int n=0,d=0;m.getTimeSignatureInfo(n,d);require(n==7 && d==8,"meter");meter=true;}
        require(m.getTimeStamp()==0,"conductor timing");
    }
    require(tempo && meter,"missing tempo/meter");
    const auto* piano=midi.getTrack(1);
    require(piano->getEventPointer(0)->message.getTextFromTextMetaEvent()==juce::String(s.tracks[1].name),"track name");
    int notes=0;bool offAt960=false,onAfterOff=false;
    for(int i=0;i<piano->getNumEvents();++i){
        const auto& m=piano->getEventPointer(i)->message;
        if(m.isNoteOn()){
            require(m.getChannel()==1 && m.getNoteNumber()==60,"piano routing/pitch");
            if(notes++==0)require(m.getTimeStamp()==118 && m.getVelocity()==64,"rounded start/velocity");
            else {require(m.getTimeStamp()==960 && m.getVelocity()==99,"second note");onAfterOff=offAt960;}
        }
        if(m.isNoteOff() && m.getTimeStamp()==960)offAt960=true;
    }
    require(notes==2 && onAfterOff,"note-off must precede same-tick retrigger");
    bool bass=false;
    for(int i=0;i<midi.getTrack(2)->getNumEvents();++i){const auto& m=midi.getTrack(2)->getEventPointer(i)->message;
        if(m.isNoteOn()){require(m.getChannel()==2 && m.getNoteNumber()==36 && m.getVelocity()==127 && m.getTimeStamp()==1920,"bass events");bass=true;}}
    require(bass,"missing second instrument");
    require(!exportSessionMidi(s,output,error) && bytes(output)==raw,"existing MIDI overwritten");
    const auto project=root.getChildFile("Session.idwproj");require(saveSession(s,project),"save source arrangement");Session reopened;
    require(loadSession(project,reopened),"reopen arrangement");const auto again=root.getChildFile("reopened.mid");
    require(exportSessionMidi(reopened,again,error) && bytes(again)==raw,"project roundtrip changed MIDI");
    auto solo=s;solo.tracks[2].solo=true;const auto soloFile=root.getChildFile("solo.mid");
    require(exportSessionMidi(solo,soloFile,error) && read(soloFile).getNumTracks()==2,"solo filter");
    const auto rejected=root.getChildFile("rejected.mid");
    const auto reject=[&](const Session& invalid){
        require(!exportSessionMidi(invalid,rejected,error) && !rejected.exists() && error.isNotEmpty(),"invalid MIDI created output");};
    auto audioSolo=s;audioSolo.tracks[0].solo=true;reject(audioSolo);
    Session empty;reject(empty);
    for(double bpm:{0.0,19.99,300.01,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
        auto bad=s;bad.bpm=bpm;reject(bad);
    }
    for(int d:{0,3,5,64}){auto bad=s;bad.denominator=d;reject(bad);}
    auto bad=s;bad.numerator=0;reject(bad);
    for(const auto& note:std::vector<MidiNote>{{-1,0,1,100},{128,0,1,100},{60,0,1,0},{60,0,1,128},
            {60,-1,1,100},{60,0,0,100},{60,0,0.000001,100},{60,0,1e300,100},
            {60,std::numeric_limits<double>::quiet_NaN(),1,100}}){
        bad=s;bad.tracks[1].notes={note};reject(bad);
    }
    Session many;
    for(int i=0;i<15;++i){many.addTrack(TrackType::midi);require(many.addMidiNote((size_t)i,{60,0,1,100}),"channel fixture");}
    const auto channels=root.getChildFile("channels.mid");require(exportSessionMidi(many,channels,error),"15 channel export");
    const auto routed=read(channels);require(routed.getNumTracks()==16,"15 tracks plus conductor");
    std::set<int> used;
    for(int t=1;t<16;++t)for(int i=0;i<routed.getTrack(t)->getNumEvents();++i){const auto& m=routed.getTrack(t)->getEventPointer(i)->message;
        if(m.isNoteOn()){require(m.getChannel()!=10 && used.insert(m.getChannel()).second,"channel collision or percussion routing");}}
    require(used.size()==15,"missing melodic channel");
    many.addTrack(TrackType::midi);many.addMidiNote(15,{60,0,1,100});reject(many);
    auto boundary=s;boundary.bpm=300;boundary.tracks[1].notes={{60,71999,1,100}};boundary.tracks[2].notes.clear();
    const auto longFile=root.getChildFile("four-hours.mid");require(exportSessionMidi(boundary,longFile,error),"four hour tick boundary");
    const auto longMidi=read(longFile);
    require(longMidi.getTrack(1)->getEndTime()==69120000,"large tick conversion");
    boundary.tracks[1].notes[0].startBeat=72000;reject(boundary);
    std::atomic<bool> cancel{true};
    require(!exportSessionMidi(s,rejected,error,nullptr,&cancel) && !rejected.exists(),"canceled export published");
    const auto blocker=root.getChildFile("blocker");require(blocker.replaceWithText("unchanged"),"blocked-parent fixture");
    require(!exportSessionMidi(s,blocker.getChildFile("failed.mid"),error) && blocker.loadFileAsString()=="unchanged","failed output modified parent");
    require(juce::JSON::toString(sessionToJson(s))==before && bytes(output)==raw,"source session or original MIDI changed");
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS standard MIDI header/events/tempo/meter/name/velocity/timing, project roundtrip, solo, channel isolation, limits, cancellation and overwrite protection\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
