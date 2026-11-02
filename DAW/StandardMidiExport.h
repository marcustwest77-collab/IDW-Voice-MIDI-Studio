#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "SessionModel.h"
#include <atomic>
#include <cstring>
namespace idw::daw {
// SMF type 1, 960 ticks/quarter. Export audible MIDI tracks on independent
// melodic channels (skip GM percussion channel 10). Audio and synth patches
// are not MIDI events. Call from a worker thread with an immutable session.
inline bool exportSessionMidi(const Session& session,const juce::File& destination,
                              juce::String& error,std::atomic<float>* progress=nullptr,
                              std::atomic<bool>* cancel=nullptr){
    error.clear();if(progress)progress->store(0,std::memory_order_relaxed);
    const auto canceled=[&]{return cancel && cancel->load(std::memory_order_relaxed);};
    if(destination==juce::File{} || destination.exists()){
        error="Choose a new MIDI filename.";return false;
    }
    if(!std::isfinite(session.bpm) || session.bpm<20 || session.bpm>300 ||
       session.numerator<1 || session.numerator>32 || session.denominator<1 ||
       session.denominator>32 || (session.denominator&(session.denominator-1))!=0){
        error="MIDI export requires a valid tempo and a power-of-two meter denominator.";return false;
    }
    const bool anySolo=std::any_of(session.tracks.begin(),session.tracks.end(),[](const auto& t){return t.solo;});
    std::vector<const Track*> tracks;
    for(const auto& t:session.tracks)
        if(t.type==TrackType::midi && !t.mute && (!anySolo || t.solo) && !t.notes.empty())tracks.push_back(&t);
    if(tracks.empty()){error="No audible MIDI notes to export.";return false;}
    if(tracks.size()>15){error="Export supports up to 15 audible MIDI tracks with separate melodic channels.";return false;}
    juce::MidiFile file;file.setTicksPerQuarterNote(960);
    juce::MidiMessageSequence conductor;
    conductor.addEvent(juce::MidiMessage::tempoMetaEvent((int)std::llround(60000000.0/session.bpm)));
    conductor.addEvent(juce::MidiMessage::timeSignatureMetaEvent(session.numerator,session.denominator));
    conductor.addEvent(juce::MidiMessage::endOfTrack());file.addTrack(conductor);
    struct Event {int tick,pitch,velocity;bool on;};
    int channel=1;
    for(size_t index=0;index<tracks.size();++index){
        const auto& t=*tracks[index];std::vector<Event> events;
        if(t.notes.size()>100000){error="Too many notes in a MIDI track.";return false;}
        events.reserve(t.notes.size()*2);
        int lastTick=0;
        for(const auto& n:t.notes){
            if(canceled()){error="MIDI export canceled.";return false;}
            const double end=n.startBeat+n.lengthBeats;
            if(n.pitch<0 || n.pitch>127 || n.velocity<1 || n.velocity>127 ||
               !std::isfinite(n.startBeat) || n.startBeat<0 || !std::isfinite(n.lengthBeats) ||
               n.lengthBeats<=0 || !std::isfinite(end) || end*60.0/session.bpm>14400){
                error="Invalid MIDI note or export longer than four hours.";return false;
            }
            const int first=(int)std::llround(n.startBeat*960),last=(int)std::llround(end*960);
            if(last<=first){error="A MIDI note rounds to zero duration at 960 ticks per quarter note.";return false;}
            events.push_back({first,n.pitch,n.velocity,true});events.push_back({last,n.pitch,0,false});
            lastTick=std::max(lastTick,last);
        }
        std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){
            if(a.tick!=b.tick)return a.tick<b.tick;
            return a.on<b.on; // Release before retrigger at the same tick.
        });
        juce::MidiMessageSequence sequence;
        sequence.addEvent(juce::MidiMessage::textMetaEvent(3,juce::String(t.name)));
        for(const auto& e:events){
            if(canceled()){error="MIDI export canceled.";return false;}
            auto message=e.on?juce::MidiMessage::noteOn(channel,e.pitch,(juce::uint8)e.velocity):
                              juce::MidiMessage::noteOff(channel,e.pitch);
            message.setTimeStamp(e.tick);sequence.addEvent(message);
        }
        auto end=juce::MidiMessage::endOfTrack();end.setTimeStamp(lastTick);sequence.addEvent(end);
        file.addTrack(sequence);if(++channel==10)++channel;
        if(progress)progress->store(0.8f*(float)(index+1)/(float)tracks.size(),std::memory_order_relaxed);
    }
    if(canceled()){error="MIDI export canceled.";return false;}
    juce::TemporaryFile staged(destination);
    {
        juce::FileOutputStream output(staged.getFile());
        if(!output.openedOk() || !file.writeTo(output,1)){
            error="Cannot write MIDI output.";return false;
        }
        output.flush();if(output.getStatus().failed()){error="Failed to flush MIDI output.";return false;}
    }
    // Read the closed file without synthesizing missing note-offs. Never publish
    // an unreadable or truncated candidate as the user's final MIDI file.
    {
        juce::FileInputStream input(staged.getFile());juce::MidiFile checked;int type=-1;
        if(!input.openedOk() || !checked.readFrom(input,false,&type) || type!=1 ||
           checked.getTimeFormat()!=960 || checked.getNumTracks()!=file.getNumTracks()){
            error="MIDI output verification failed.";return false;
        }
        for(int track=0;track<file.getNumTracks();++track){
            const auto* a=file.getTrack(track);const auto* b=checked.getTrack(track);
            if(a->getNumEvents()!=b->getNumEvents()){error="MIDI event verification failed.";return false;}
            for(int i=0;i<a->getNumEvents();++i){
                const auto& original=a->getEventPointer(i)->message;
                const auto& saved=b->getEventPointer(i)->message;
                if(original.getTimeStamp()!=saved.getTimeStamp() || original.getRawDataSize()!=saved.getRawDataSize() ||
                   std::memcmp(original.getRawData(),saved.getRawData(),(size_t)original.getRawDataSize())!=0){
                    error="MIDI event verification failed.";return false;
                }
            }
        }
    }
    if(canceled()){error="MIDI export canceled.";return false;}
    if(destination.exists() || !staged.overwriteTargetFileWithTemporary()){
        error="Cannot publish MIDI file; choose a new filename.";return false;
    }
    if(progress)progress->store(1,std::memory_order_relaxed);return true;
}
}
