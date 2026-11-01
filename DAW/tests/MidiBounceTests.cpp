#include "OfflineBounce.h"
#include "SessionPersistence.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idw::daw;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static juce::AudioBuffer<float> synth(const Session& s,int frames,int rate,int block){
    juce::AudioBuffer<float> result(2,frames);result.clear();
    for(int pos=0;pos<frames;pos+=block)
        renderMidiSine(s,result.getWritePointer(0)+pos,result.getWritePointer(1)+pos,
                       std::min(block,frames-pos),pos,rate);
    return result;
}
static juce::AudioBuffer<float> read(const juce::File& file,int frames,int rate){
    juce::WavAudioFormat wav;
    auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(file.createInputStream().release(),true));
    require(reader && reader->numChannels==2 && reader->bitsPerSample==24 &&
            reader->sampleRate==rate && reader->lengthInSamples==frames,"WAV metadata/tail");
    juce::AudioBuffer<float> result(2,frames);
    require(reader->read(&result,0,frames,0,true,true),"read exported samples");return result;
}
static void compare(const juce::AudioBuffer<float>& a,const juce::AudioBuffer<float>& b,float tolerance){
    for(int ch=0;ch<2;++ch)for(int i=0;i<a.getNumSamples();++i)
        require(std::isfinite(a.getSample(ch,i)) &&
                std::abs(a.getSample(ch,i)-b.getSample(ch,i))<=tolerance,"sample mismatch");
}
int main(){
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("idw-midi-bounce","",false);
    require(root.createDirectory().wasOk(),"fixture directory");
    for(int rate:{44100,48000})for(int preset=0;preset<5;++preset){
        Session s;s.bpm=60;s.addTrack(TrackType::midi);
        s.tracks[0].synthPreset=preset;s.tracks[0].gain=0.7f;s.tracks[0].pan=-0.35f;
        require(s.addMidiNote(0,{60,0.01,0.29,97}),"first note");
        require(s.addMidiNote(0,{67,0.053,0.081,71}),"overlapping note");
        require(s.addMidiNote(0,{72,0.161,0.039,110}),"later note");
        s.addTrack(TrackType::midi);s.tracks[1].pan=0.8f;
        require(s.addMidiNote(1,{48,0.111,0.089,85}),"second instrument");
        // Persist/reopen the actual instrument and note settings before export.
        const auto project=root.getChildFile("session.idwproj");
        require(saveSession(s,project),"save MIDI project");Session reopened;
        require(loadSession(project,reopened),"reopen MIDI project");
        const int frames=(int)std::ceil(0.3*rate);
        const auto reference=synth(s,frames,rate,1);
        for(int block:{17,64,127,256,1024,2048})
            compare(synth(reopened,frames,rate,block),reference,0.0000001f);
        const auto output=root.getChildFile("midi-"+juce::String(rate)+"-"+juce::String(preset)+".wav");
        juce::String error;std::atomic<float> progress{0};
        require(renderSessionWav(reopened,output,error,rate,1,-1,0,0,&progress),"MIDI-only export");
        require(progress.load()==1.0f,"completed progress");
        compare(read(output,frames,rate),reference,0.000001f);
        require(reference.getMagnitude(0,frames)>0.01f,"silent MIDI fixture");
        // Existing output must survive a refused overwrite.
        juce::MemoryBlock bytes,after;require(output.loadFileAsData(bytes),"original WAV");
        require(!renderSessionWav(reopened,output,error,rate),"overwrite accepted");
        require(output.loadFileAsData(after) && bytes==after,"overwrite changed WAV");
        auto muted=reopened;for(auto& t:muted.tracks)t.mute=true;
        const auto muteFile=root.getChildFile("mute-"+output.getFileName());
        require(renderSessionWav(muted,muteFile,error,rate),"muted MIDI export");
        require(read(muteFile,frames,rate).getMagnitude(0,frames)==0,"muted MIDI audible");
        auto solo=reopened;solo.tracks[0].solo=true;
        auto only=solo;only.tracks[1].mute=true;
        compare(synth(solo,frames,rate,31),synth(only,frames,rate,128),0);
        const auto soloFile=root.getChildFile("solo-"+output.getFileName());
        require(renderSessionWav(solo,soloFile,error,rate),"solo MIDI export");
        compare(read(soloFile,frames,rate),synth(only,frames,rate,31),0.000001f);
    }
    // Independent single-tone oracle: pitch, velocity, envelope, pan and level.
    Session single;single.bpm=60;single.addTrack(TrackType::midi);
    single.tracks[0].pan=1;single.tracks[0].gain=0.5f;
    require(single.addMidiNote(0,{69,0,0.25,127}),"oracle note");
    auto tone=synth(single,12000,48000,127);
    for(int i=240;i<11760;++i){
        require(tone.getSample(0,i)==0,"hard-right pan leaked");
        const float expected=0.06f*(float)std::sin(i*6.283185307179586*440.0/48000);
        require(std::abs(tone.getSample(1,i)-expected)<0.000001f,"independent sine oracle");
    }
    auto doubled=single;doubled.tracks[0].notes.push_back(doubled.tracks[0].notes[0]);
    const auto chord=synth(doubled,12000,48000,101);
    for(int i=240;i<11760;++i)
        require(std::abs(chord.getSample(1,i)-tone.getSample(1,i)*std::sqrt(2.0f))<0.000001f,
                "independent simultaneous voice normalization");
    // Mixed audio + MIDI must retain the longer MIDI tail and sum before master gain.
    const auto source=root.getChildFile("source.wav");
    juce::WavAudioFormat wav;auto stream=std::unique_ptr<juce::FileOutputStream>(source.createOutputStream());
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.get(),48000,2,24,{},0));
    require(writer!=nullptr,"audio fixture writer");stream.release();
    juce::AudioBuffer<float> constant(2,6000);
    for(int ch=0;ch<2;++ch)for(int i=0;i<6000;++i)constant.setSample(ch,i,0.0625f);
    require(writer->writeFromAudioSampleBuffer(constant,0,6000),"audio fixture write");writer.reset();
    single.addTrack(TrackType::audio);
    require(single.addClip(1,{source.getFullPathName().toStdString(),0,0.125,0}),"audio clip");
    juce::String error;const auto mixed=root.getChildFile("mixed.wav");
    require(renderSessionWav(single,mixed,error,48000,0.5f),"mixed export");
    auto actual=read(mixed,12000,48000);
    for(int ch=0;ch<2;++ch)for(int i=0;i<12000;++i){
        const float expected=(tone.getSample(ch,i)+(i<6000?0.0625f:0))*0.5f;
        require(std::abs(actual.getSample(ch,i)-expected)<0.000001f,"mixed/master samples");
    }
    auto audioSolo=single;audioSolo.tracks[1].solo=true;
    const auto audioOnly=root.getChildFile("audio-solo.wav");
    require(renderSessionWav(audioSolo,audioOnly,error,48000),"audio solo export");
    const auto audioResult=read(audioOnly,12000,48000);
    for(int ch=0;ch<2;++ch)for(int i=0;i<12000;++i)
        require(std::abs(audioResult.getSample(ch,i)-(i<6000?0.0625f:0))<0.000001f,
                "audio solo failed to suppress MIDI");
    const auto rejected=root.getChildFile("rejected.wav");
    auto invalid=single;invalid.tracks[0].notes[0].lengthBeats=std::numeric_limits<double>::infinity();
    require(!renderSessionWav(invalid,rejected,error) && !rejected.exists(),"nonfinite note accepted");
    invalid=single;invalid.tracks[0].notes[0].pitch=128;
    require(!renderSessionWav(invalid,rejected,error) && !rejected.exists(),"bad pitch accepted");
    invalid=single;invalid.tracks[0].synthAttackMs=std::numeric_limits<float>::quiet_NaN();
    require(!renderSessionWav(invalid,rejected,error) && !rejected.exists(),"bad envelope accepted");
    invalid=single;invalid.tracks[0].notes[0].lengthBeats=20000;
    require(!renderSessionWav(invalid,rejected,error) && !rejected.exists(),"duration limit bypass");
    std::atomic<bool> canceled{true};
    require(!renderSessionWav(single,rejected,error,44100,1,-1,0,0,nullptr,&canceled) &&
            !rejected.exists(),"canceled MIDI export left output");
    require(root.deleteRecursively(),"fixture cleanup");
    std::cout<<"PASS MIDI-only/mixed WAV, five presets, sample oracle, buffer independence, persistence, mute/solo, master, limits/cancel/overwrite\n";
}
