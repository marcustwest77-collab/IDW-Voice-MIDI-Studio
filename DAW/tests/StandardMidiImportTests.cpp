#include "StandardMidiImport.h"
#include "StandardMidiExport.h"
#include "MidiEditHistory.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
using Bytes=std::vector<uint8_t>;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void u16(Bytes& b,unsigned v){b.push_back((uint8_t)(v>>8));b.push_back((uint8_t)v);}
static void u32(Bytes& b,size_t v){for(int n=3;n>=0;--n)b.push_back((uint8_t)(v>>(8*n)));}
static void vlq(Bytes& b,uint32_t v){uint8_t a[4];int n=0;a[n++]=(uint8_t)(v&127);while((v>>=7)!=0)a[n++]=(uint8_t)((v&127)|128);while(n)b.push_back(a[--n]);}
static void event(Bytes& b,uint32_t delta,std::initializer_list<uint8_t> data){vlq(b,delta);b.insert(b.end(),data);}
static void end(Bytes& b){event(b,0,{255,47,0});}
static Bytes smf(unsigned format,unsigned division,const std::vector<Bytes>& tracks){
    Bytes b{'M','T','h','d'};u32(b,6);u16(b,format);u16(b,(unsigned)tracks.size());u16(b,division);
    for(const auto& t:tracks){b.insert(b.end(),{'M','T','r','k'});u32(b,t.size());b.insert(b.end(),t.begin(),t.end());}return b;
}
static Bytes notes(){Bytes b;event(b,0,{144,60,30});event(b,960,{128,60,64});event(b,960,{144,60,70});event(b,960,{144,60,0});end(b);return b;}
static bool same(const ImportedMidiFile& a,const ImportedMidiFile& b){
    if(a.format!=b.format || a.ticksPerQuarter!=b.ticksPerQuarter || a.bpm!=b.bpm ||
       a.numerator!=b.numerator || a.denominator!=b.denominator || a.tracks.size()!=b.tracks.size())return false;
    for(size_t i=0;i<a.tracks.size();++i){const auto& x=a.tracks[i];const auto& y=b.tracks[i];
        if(x.name!=y.name || x.sourceTrack!=y.sourceTrack || x.sourceChannel!=y.sourceChannel || !sameMidiNotes(x.notes,y.notes))return false;
    }return true;
}
int main(int argc,char** argv){try{
    juce::String error;ImportedMidiFile decoded;
    const auto valid=smf(0,960,{notes()});
    require(decodeStandardMidi(valid.data(),valid.size(),decoded,error),"format 0 import");
    require(error.isEmpty() && decoded.tracks.size()==1 && decoded.noteCount()==2 && decoded.bpm==120 &&
        decoded.numerator==4 && decoded.denominator==4 && decoded.tracks[0].sourceChannel==1,"default MIDI metadata");
    require(decoded.tracks[0].notes[0].velocity==30 && decoded.tracks[0].notes[1].velocity==70 &&
        decoded.tracks[0].notes[1].startBeat==2 && decoded.tracks[0].notes[1].lengthBeats==1,"note timing and velocity");
    const auto seed=decoded;
    const auto reject=[&](const Bytes& bytes,const char* why){
        auto result=seed;error="prior error";
        require(!decodeStandardMidi(bytes.data(),bytes.size(),result,error),why);
        require(!error.isEmpty() && same(result,seed),"failed decode replaced output or omitted error");
    };
    for(size_t n=0;n<valid.size();++n)reject(Bytes(valid.begin(),valid.begin()+(std::ptrdiff_t)n),"truncated MIDI accepted");
    auto bad=valid;bad[0]='X';reject(bad,"bad magic");bad=valid;bad[7]=7;reject(bad,"extended header accepted");
    bad=valid;bad[9]=2;reject(bad,"asynchronous format 2");bad=valid;bad[11]=0;reject(bad,"zero track count");
    bad=valid;bad[12]=128;reject(bad,"SMPTE division");bad=valid;bad[12]=bad[13]=0;reject(bad,"zero PPQ");
    bad=valid;bad.push_back(0);reject(bad,"trailing bytes");bad=valid;bad[18]=127;reject(bad,"oversized track chunk");
    reject(smf(0,960,{notes(),notes()}),"format 0 multiple tracks");
    reject(smf(1,960,std::vector<Bytes>(129,Bytes{0,255,47,0})),"source track limit");
    reject(Bytes(ImportedMidiFile::maxBytes+1),"file size bound");
    Bytes t;event(t,0,{128,60,0});end(t);reject(smf(0,960,{t}),"orphan note-off");
    t.clear();event(t,0,{144,60,100});end(t);reject(smf(0,960,{t}),"unfinished note");
    t.clear();event(t,0,{144,60,100});event(t,0,{128,60,0});end(t);reject(smf(0,960,{t}),"zero duration note");
    t.clear();event(t,0,{144,128,100});end(t);reject(smf(0,960,{t}),"non-seven-bit note");
    t.clear();event(t,0,{60,100});end(t);reject(smf(0,960,{t}),"missing running status");
    t={128,128,128,128,0,255,47,0};reject(smf(0,960,{t}),"five-byte VLQ");
    for(const auto& unsupported:std::vector<Bytes>{{0,176,64,127},{0,192,5},{0,224,0,64},{0,240,0},{0,248}}){
        t=unsupported;end(t);reject(smf(0,960,{t}),"unsupported performance event");
    }
    t=notes();t.insert(t.end(),{0,255,1,0});reject(smf(0,960,{t}),"event after EOT");
    t=notes();t.resize(t.size()-4);reject(smf(0,960,{t}),"missing EOT");
    t={0,255,47,1,0};reject(smf(0,960,{t}),"invalid EOT length");
    t={0,255,81,2,7,161};end(t);reject(smf(0,960,{t}),"invalid tempo metadata");
    t={0,255,88,4,4,6,24,8};end(t);reject(smf(0,960,{t}),"unsupported meter denominator");
    t={0,255,84,5,0,0,0,0,0};end(t);reject(smf(0,960,{t}),"SMPTE offset");
    t={0,255,47,0};reject(smf(0,960,{t}),"no notes");
    // Constant metadata can occur in a later source track; no implicit tempo map.
    Bytes conductor{0,255,81,3,9,39,192,0,255,88,4,6,3,24,8};end(conductor); // 100 BPM, 6/8
    auto b=smf(1,960,{notes(),conductor});
    require(decodeStandardMidi(b.data(),b.size(),decoded,error) && decoded.bpm==100 &&
        decoded.numerator==6 && decoded.denominator==8,"constant file metadata");
    auto dynamic=conductor;dynamic.resize(dynamic.size()-4);event(dynamic,960,{255,81,3,7,161,32});end(dynamic);
    reject(smf(1,960,{dynamic,notes()}),"changing tempo accepted");
    dynamic=conductor;dynamic.resize(dynamic.size()-4);event(dynamic,960,{255,88,4,4,2,24,8});end(dynamic);
    reject(smf(1,960,{dynamic,notes()}),"changing meter accepted");
    // Running status, velocity-zero release, separate channels and FIFO overlaps.
    t={0,255,3,4,'L','e','a','d'};
    event(t,0,{144,60,40});event(t,240,{60,100});event(t,240,{60,0});event(t,240,{60,0});
    event(t,0,{153,36,90});event(t,480,{137,36,64});end(t);b=smf(0,480,{t});
    require(decodeStandardMidi(b.data(),b.size(),decoded,error) && decoded.tracks.size()==2 && decoded.noteCount()==3,"channel/running import");
    require(decoded.tracks[0].name=="Lead (ch 1)" && decoded.tracks[1].sourceChannel==10 &&
        decoded.tracks[0].notes[0].startBeat==0 && decoded.tracks[0].notes[0].lengthBeats==1 &&
        decoded.tracks[0].notes[1].startBeat==0.5 && decoded.tracks[0].notes[1].lengthBeats==1 &&
        decoded.tracks[0].notes[1].velocity==100,"overlap pairing or note velocity");
    t.clear();for(size_t i=0;i<=ImportedMidiFile::maxEvents;++i)event(t,0,{255,1,0});end(t);
    reject(smf(0,960,{t}),"event count limit");
    t.clear();for(size_t i=0;i<=ImportedMidiFile::maxNotes;++i)event(t,0,{144,60,100});end(t);
    reject(smf(0,960,{t}),"note count limit");
    std::vector<Bytes> many;
    for(int i=0;i<9;++i){t.clear();for(int ch=0;ch<16;++ch){event(t,0,{(uint8_t)(144+ch),60,100});event(t,1,{(uint8_t)(128+ch),60,0});}end(t);many.push_back(t);}
    reject(smf(1,960,many),"destination channel-track count limit");
    // Mutations must either fail atomically or produce data the native model
    // can stage/commit, never a half-decoded import.
    uint32_t randomState=0x19d52bu;
    for(int i=0;i<1500;++i){
        Bytes candidate=valid;
        if(i>=1000)candidate.assign((size_t)(i%257),0);
        for(int n=0;n<(i%5)+1 && !candidate.empty();++n){
            randomState=randomState*1664525u+1013904223u;
            candidate[(size_t)randomState%candidate.size()]^=(uint8_t)(1u<<(randomState%8));
        }
        auto result=seed;
        if(decodeStandardMidi(candidate.data(),candidate.size(),result,error)){
            Session target;target.bpm=result.bpm;
            auto accepted=MidiImportPlan::prepare(target,result,error);
            require(accepted && accepted->commit(target,error),"accepted mutation could not enter native model");
        }else require(same(result,seed) && !error.isEmpty(),"mutation failure changed output");
    }
    // A preview is read-only; commit appends tracks without replacing audio or mixer work.
    Session session;session.bpm=93;session.numerator=7;session.denominator=8;
    session.masterMix={0.7,-3,-2,1};session.addTrack(TrackType::audio);session.addTrack(TrackType::midi);
    session.tracks[0].gain=0.4f;session.tracks[0].inputChannel=1;session.tracks[1].synthPreset=4;
    session.addClip(0,{"existing.wav",3,4,0.5});session.addMidiNote(1,{67,8,2,80});
    const auto before=juce::JSON::toString(sessionToJson(session));
    auto plan=MidiImportPlan::prepare(session,seed,error);
    require(plan && plan->firstTrack()==2 && plan->trackCount()==1 && plan->noteCount()==2 &&
        juce::JSON::toString(sessionToJson(session))==before,"preview mutated session");
    auto changed=session;changed.masterMix.gain=0.3;const auto newer=juce::JSON::toString(sessionToJson(changed));
    require(!plan->commit(changed,error) && juce::JSON::toString(sessionToJson(changed))==newer,"stale preview overwrote newer project");
    require(plan->commit(session,error) && session.tracks.size()==3 && session.bpm==93 && session.numerator==7 &&
        session.denominator==8 && session.masterMix.gain==0.7 && sameMidiNotes(session.tracks[1].notes,changed.tracks[1].notes) &&
        session.tracks[0].clips[0].sourcePath=="existing.wav" && session.tracks[0].gain==0.4f &&
        session.tracks[2].synthPreset==0 && sameMidiNotes(session.tracks[2].notes,seed.tracks[0].notes),"append changed existing work");
    auto preserved=session;preserved.tracks.resize(2);
    require(juce::JSON::toString(sessionToJson(preserved))==before,"append changed existing project metadata");
    const auto committed=juce::JSON::toString(sessionToJson(session));
    require(!plan->commit(session,error) && juce::JSON::toString(sessionToJson(session))==committed,"preview committed twice");
    Session full;for(int i=0;i<128;++i)full.addTrack(TrackType::midi);
    require(!MidiImportPlan::prepare(full,seed,error),"full session accepted import");
    auto invalid=seed;invalid.tracks[0].notes[0].pitch=128;require(!MidiImportPlan::prepare(session,invalid,error),"invalid staged note");
    invalid=seed;invalid.tracks[0].notes[0].lengthBeats=std::numeric_limits<double>::infinity();
    require(!MidiImportPlan::prepare(session,invalid,error),"nonfinite staged duration");
    invalid=seed;invalid.tracks[0].notes[0].startBeat=6000;Session slow;slow.bpm=20;
    require(!MidiImportPlan::prepare(slow,invalid,error),"four-hour project-tempo bound");
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("idw-midi-import","",false);
    require(root.createDirectory().wasOk(),"test folder");
    const auto file=root.getChildFile("notes.mid");require(file.replaceWithData(valid.data(),valid.size()),"test MIDI write");
    require(readStandardMidi(file,decoded,error) && same(decoded,seed),"file import");
    auto unchanged=seed;require(!readStandardMidi(root.getChildFile("missing.mid"),unchanged,error) && same(unchanged,seed),"missing file changed output");
    const Bytes oversized(ImportedMidiFile::maxBytes+2);const auto large=root.getChildFile("large.mid");
    require(large.replaceWithData(oversized.data(),oversized.size()),"large fixture");
    require(!readStandardMidi(large,unchanged,error) && same(unchanged,seed),"file read bound");
    Session source;source.addTrack(TrackType::midi);source.tracks[0].name="Roundtrip";source.tracks[0].notes=seed.tracks[0].notes;
    const auto exported=root.getChildFile("export.mid");require(exportSessionMidi(source,exported,error),"native MIDI export");
    require(readStandardMidi(exported,decoded,error) && decoded.noteCount()==2 && sameMidiNotes(decoded.tracks[0].notes,source.tracks[0].notes),"native MIDI export/import roundtrip");
    const auto project=root.getChildFile("Session.idwproj");Session recalled;
    require(saveSession(session,project) && loadSession(project,recalled) &&
        juce::JSON::toString(sessionToJson(recalled))==committed,"imported notes save/reopen");
    require(root.getParentDirectory()==juce::File::getSpecialLocation(juce::File::tempDirectory),"cleanup outside test temp root");
    require(root.deleteRecursively(),"test folder cleanup");
    if(argc>1){
        const auto actual=juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(argv[1]));
        require(readStandardMidi(actual,decoded,error),"actual exported MIDI import");
        require(decoded.tracks.size()==1 && decoded.noteCount()==2 && decoded.bpm==120 &&
            decoded.tracks[0].notes[0].velocity==30 && decoded.tracks[0].notes[1].velocity==70,"actual export differs from expected two-note fixture");
        std::cout<<"PASS actual local exported MIDI read-only: two C4 notes, 120 BPM, velocities 30/70\n";
    }
    std::cout<<"PASS MIDI import: format 0/1, channels/running status/FIFO pairing, bounds and malformed input, constant metadata, atomic/stale preview, preserved audio/mix, native export/import and project recall; no drivers\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
