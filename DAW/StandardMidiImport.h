#pragma once
#include "SessionPersistence.h"
#include <deque>
#include <map>
#include <optional>
namespace idw::daw {
struct ImportedMidiTrack {
    std::string name;
    int sourceTrack=0,sourceChannel=1;
    std::vector<MidiNote> notes;
};
struct ImportedMidiFile {
    static constexpr size_t maxBytes=1024*1024,maxEvents=100000,maxNotes=50000,maxTracks=128;
    int format=1,ticksPerQuarter=960,numerator=4,denominator=4;
    double bpm=120;
    std::vector<ImportedMidiTrack> tracks;
    size_t noteCount() const {size_t n=0;for(const auto& t:tracks)n+=t.notes.size();return n;}
};
namespace midi_import_detail {
struct Reader {
    const uint8_t* data;size_t size,pos=0;
    size_t remaining() const {return size-pos;}
    bool byte(uint8_t& v){if(!remaining())return false;v=data[pos++];return true;}
    bool u16(uint16_t& v){uint8_t a,b;if(!byte(a)||!byte(b))return false;v=(uint16_t)((a<<8)|b);return true;}
    bool u32(uint32_t& v){v=0;for(int i=0;i<4;++i){uint8_t b;if(!byte(b))return false;v=(v<<8)|b;}return true;}
    bool vlq(uint32_t& v){v=0;for(int i=0;i<4;++i){uint8_t b;if(!byte(b))return false;v=(v<<7)|(b&127u);if(!(b&128u))return true;}return false;}
};
struct Pending {uint64_t tick;int velocity;};
struct Tempo {uint64_t tick;uint32_t value;};
struct Meter {uint64_t tick;int numerator,denominator;};
}
// SMF 0/1 with PPQ timing, paired note events and a constant tempo/meter.
// Parse into temporary storage; errors never replace the caller's result.
inline bool decodeStandardMidi(const void* bytes,size_t size,ImportedMidiFile& output,juce::String& error){
    using namespace midi_import_detail;
    error.clear();const auto fail=[&](const char* message){error=message;return false;};
    if(!bytes || size<14 || size>ImportedMidiFile::maxBytes)return fail("Choose a MIDI file between 14 bytes and 1 MiB.");
    Reader r{static_cast<const uint8_t*>(bytes),size};
    uint32_t magic,length;uint16_t format,count,division;
    if(!r.u32(magic)||magic!=0x4d546864u||!r.u32(length)||length!=6||
       !r.u16(format)||!r.u16(count)||!r.u16(division))return fail("Invalid MIDI header.");
    if(format>1 || !count || count>ImportedMidiFile::maxTracks || (format==0 && count!=1))
        return fail("Import supports MIDI format 0 or 1, with up to 128 source tracks.");
    if(!division || (division&0x8000u))return fail("Import requires PPQ timing; SMPTE timing is not supported.");
    ImportedMidiFile next;next.format=format;next.ticksPerQuarter=division;
    std::vector<Tempo> tempos;std::vector<Meter> meters;
    size_t eventCount=0,noteCount=0;
    for(int ti=0;ti<count;++ti){
        if(!r.u32(magic)||magic!=0x4d54726bu||!r.u32(length)||length>r.remaining())return fail("Invalid or truncated MIDI track.");
        Reader track{r.data+r.pos,length};r.pos+=length;
        uint64_t tick=0;uint8_t running=0;bool ended=false;
        juce::String name="Imported MIDI "+juce::String(ti+1);
        std::map<int,std::deque<Pending>> active;
        std::map<int,std::vector<MidiNote>> channels;
        while(track.remaining()){
            if(ended)return fail("MIDI data follows an end-of-track event.");
            if(++eventCount>ImportedMidiFile::maxEvents)return fail("MIDI file exceeds 100000 events.");
            uint32_t delta;if(!track.vlq(delta))return fail("Invalid MIDI delta time.");
            tick+=delta;
            if(tick>72000ULL*division)return fail("MIDI timeline exceeds the supported duration.");
            uint8_t status;if(!track.remaining())return fail("Missing MIDI event.");
            if(track.data[track.pos]&128u){if(!track.byte(status))return fail("Missing MIDI status.");}
            else {if(!running)return fail("MIDI running status has no preceding channel event.");status=running;}
            if(status==255){
                running=0;uint8_t type;uint32_t n;
                if(!track.byte(type)||type>=128||!track.vlq(n)||n>track.remaining())return fail("Invalid MIDI metadata.");
                const auto* data=track.data+track.pos;track.pos+=n;
                if(type==0x2f){if(n!=0)return fail("Invalid end-of-track event.");ended=true;}
                else if(type==0x51){
                    if(n!=3)return fail("Invalid MIDI tempo event.");
                    const uint32_t value=((uint32_t)data[0]<<16)|((uint32_t)data[1]<<8)|data[2];
                    if(value<200000 || value>3000000)return fail("MIDI tempo must be between 20 and 300 BPM.");
                    tempos.push_back({tick,value});
                }else if(type==0x58){
                    if(n!=4 || data[0]<1 || data[0]>32 || data[1]>5)return fail("Unsupported MIDI time signature.");
                    meters.push_back({tick,data[0],1<<data[1]});
                }else if(type==0x54)return fail("MIDI SMPTE offsets are not supported.");
                else if(type==3 && n && juce::CharPointer_UTF8::isValidString((const char*)data,(int)n))
                    name=juce::String::fromUTF8((const char*)data,(int)n).substring(0,64).replaceCharacters("\r\n\t","   ");
                continue;
            }
            if(status>=240)return fail("System/SysEx events are not supported by notes-only MIDI import.");
            running=status;const int kind=status&240,channel=status&15;
            if(kind!=128 && kind!=144)return fail("This file uses controllers, programs or expression events; notes-only import cannot preserve them.");
            uint8_t pitch,velocity;
            if(!track.byte(pitch)||!track.byte(velocity)||pitch>127||velocity>127)return fail("Invalid or truncated MIDI note.");
            const int key=channel*128+pitch;
            if(kind==144 && velocity){
                if(++noteCount>ImportedMidiFile::maxNotes)return fail("MIDI import supports up to 50000 notes.");
                active[key].push_back({tick,velocity});
            }else{
                auto found=active.find(key);
                if(found==active.end() || found->second.empty())return fail("MIDI note-off has no matching note-on.");
                const auto start=found->second.front();found->second.pop_front();
                if(tick<=start.tick)return fail("MIDI notes must have a positive duration.");
                channels[channel].push_back({pitch,(double)start.tick/division,(double)(tick-start.tick)/division,start.velocity});
            }
        }
        if(!ended)return fail("MIDI track is missing its end-of-track event.");
        for(const auto& pending:active)if(!pending.second.empty())return fail("MIDI track contains an unfinished note.");
        for(auto& channel:channels){
            if(next.tracks.size()>=ImportedMidiFile::maxTracks)return fail("MIDI channels require more than 128 destination tracks.");
            auto& notes=channel.second;
            std::stable_sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return a.startBeat<b.startBeat;});
            next.tracks.push_back({(name.isEmpty()?juce::String("Imported MIDI"):name).toStdString()+" (ch "+std::to_string(channel.first+1)+")",ti,channel.first+1,std::move(notes)});
        }
    }
    if(r.remaining())return fail("Unexpected data after the MIDI tracks.");
    uint32_t tempo=500000;bool explicitTempo=false;
    for(const auto& t:tempos)if(!t.tick){
        if(explicitTempo && tempo!=t.value)return fail("Conflicting MIDI tempos are not supported.");
        tempo=t.value;explicitTempo=true;
    }
    for(const auto& t:tempos)if(t.value!=tempo)return fail("Tempo changes are not supported by this project format.");
    bool explicitMeter=false;
    for(const auto& m:meters)if(!m.tick){
        if(explicitMeter && (next.numerator!=m.numerator || next.denominator!=m.denominator))return fail("Conflicting MIDI time signatures are not supported.");
        next.numerator=m.numerator;next.denominator=m.denominator;explicitMeter=true;
    }
    for(const auto& m:meters)if(next.numerator!=m.numerator || next.denominator!=m.denominator)return fail("Time-signature changes are not supported by this project format.");
    next.bpm=60000000.0/tempo;
    if(next.tracks.empty())return fail("The MIDI file contains no notes.");
    for(const auto& t:next.tracks)for(const auto& n:t.notes)
        if((n.startBeat+n.lengthBeats)*60.0/next.bpm>14400)return fail("MIDI notes exceed four hours at the file tempo.");
    output=std::move(next);return true;
}
inline bool readStandardMidi(const juce::File& file,ImportedMidiFile& output,juce::String& error){
    juce::FileInputStream input(file);
    if(!input.openedOk()){error="Could not open the MIDI file.";return false;}
    juce::MemoryBlock bytes;input.readIntoMemoryBlock(bytes,static_cast<int>(ImportedMidiFile::maxBytes+1));
    return decodeStandardMidi(bytes.getData(),bytes.getSize(),output,error);
}
// Prepared state is private so callers cannot modify the preview after validation.
// Commit rejects a changed destination; adding tracks is a single model replacement.
class MidiImportPlan {
public:
    static std::optional<MidiImportPlan> prepare(const Session& current,const ImportedMidiFile& file,juce::String& error){
        error.clear();
        if(!std::isfinite(current.bpm)||current.bpm<20||current.bpm>300||file.tracks.empty()||
           file.tracks.size()>128 || current.tracks.size()>128-file.tracks.size()){
            error="The project cannot accept these MIDI tracks (128-track maximum).";return {};
        }
        MidiImportPlan plan;plan.before=juce::JSON::toString(sessionToJson(current));plan.staged=current;
        plan.first=current.tracks.size();size_t notes=0;
        for(const auto& source:file.tracks){
            if(source.notes.empty() || source.notes.size()>ImportedMidiFile::maxNotes-notes){error="Invalid or oversized imported note list.";return {};}
            notes+=source.notes.size();
            if(!plan.staged.addTrack(TrackType::midi)){error="Could not add MIDI track.";return {};}
            const auto index=plan.staged.tracks.size()-1;
            plan.staged.tracks[index].name=source.name;
            for(const auto& note:source.notes){
                const double end=note.startBeat+note.lengthBeats;
                if(!std::isfinite(end) || end<=note.startBeat || end*60/current.bpm>14400 || !plan.staged.addMidiNote(index,note)){
                    error="Imported notes are invalid or exceed four hours at the project tempo.";return {};
                }
            }
        }
        plan.notes=notes;return plan;
    }
    size_t firstTrack() const {return first;}
    size_t trackCount() const {return staged.tracks.size()-first;}
    size_t noteCount() const {return notes;}
    bool commit(Session& current,juce::String& error) const {
        if(juce::JSON::toString(sessionToJson(current))!=before){error="The project changed during import. Import the MIDI file again.";return false;}
        auto replacement=staged;current=std::move(replacement);error.clear();return true;
    }
private:
    Session staged;juce::String before;size_t first=0,notes=0;
};
}
