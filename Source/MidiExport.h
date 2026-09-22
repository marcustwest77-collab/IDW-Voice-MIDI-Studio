#pragma once
#include "PerformanceCapture.h"
#include <cmath>
inline bool writePerformanceMidi(const juce::File& file, const std::vector<PerformanceCapture::Event>& take, double takeBpm){
    juce::MidiMessageSequence sequence;sequence.addEvent(juce::MidiMessage::tempoMetaEvent((int)std::lround(60000000.0/takeBpm)));sequence.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4,4));
    std::array<std::array<bool,128>,16> held{};double lastTime=0;
    for(const auto& e:take){juce::MidiMessage msg(e.data,e.size,e.seconds*takeBpm*960.0/60.0);lastTime=msg.getTimeStamp();
        if(msg.isNoteOn())held[(size_t)msg.getChannel()-1][(size_t)msg.getNoteNumber()]=true;
        if(msg.isNoteOff())held[(size_t)msg.getChannel()-1][(size_t)msg.getNoteNumber()]=false;
        if(msg.isAllNotesOff()||msg.isAllSoundOff()){
            for(int n=0;n<128;++n)if(held[(size_t)msg.getChannel()-1][(size_t)n]){auto off=juce::MidiMessage::noteOff(msg.getChannel(),n);off.setTimeStamp(lastTime);sequence.addEvent(off);held[(size_t)msg.getChannel()-1][(size_t)n]=false;}
        }
        sequence.addEvent(msg);
    }
    for(int ch=0;ch<16;++ch)for(int n=0;n<128;++n)if(held[(size_t)ch][(size_t)n]){auto off=juce::MidiMessage::noteOff(ch+1,n);off.setTimeStamp(lastTime+1);sequence.addEvent(off);}
    sequence.updateMatchedPairs();juce::MidiFile midiFile;midiFile.setTicksPerQuarterNote(960);midiFile.addTrack(sequence);
    juce::MemoryOutputStream stream;if(!midiFile.writeTo(stream,0))return false;
    return file.replaceWithData(stream.getData(),stream.getDataSize());
}
