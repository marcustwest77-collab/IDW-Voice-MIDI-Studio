#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "SongPattern.h"
#include "StudioSynth.h"
#include "ReleaseSafety.h"

// One baked stereo performance track plus a sample-clocked repeating drum pattern.
// Disk IO and buffer allocation occur only on the UI thread while transport is stopped.
class SongStudio {
public:
    void prepare(double nextRate){const juce::ScopedLock guard(lock);playing=false;recording=false;rate=nextRate;displayRate=nextRate;drums.prepare(rate);configureDrums(drums);}
    bool isPlaying()const{return playing.load();}
    bool isRecording()const{return recording.load();}
    double seconds()const{return position.load()/displayRate.load();}
    bool hasTake()const{const juce::ScopedLock guard(lock);return recorded>0;}
    idw::SongPattern pattern()const{const juce::ScopedLock guard(lock);return song;}
    bool setPattern(idw::SongPattern next){const juce::ScopedLock guard(lock);if(playing||!next.valid())return false;song=next;return true;}
    bool start(bool record){
        const juce::ScopedLock guard(lock);
        if(playing||rate<8000||rate>192000||!song.valid()||(record&&recorded>0))return false;
        if(record){try{track.setSize(2,(int)song.samples(rate));}catch(const std::bad_alloc&){return false;}track.clear();recorded=0;takeRate=rate;}
        drums.reset();lastStep=-1;position=0;recording=record;playing=true;return true;
    }
    void stop(){playing=false;recording=false;}
    void clearTake(){const juce::ScopedLock guard(lock);if(!playing){recorded=0;track.setSize(0,0);}}
    void process(juce::AudioBuffer<float>& output){
        // UI actions never block the audio callback. UI edits are disabled in transport.
        const juce::ScopedTryLock guard(lock);if(!guard.isLocked()||!playing)return;
        auto at=position.load();const bool rec=recording.load();const auto end=song.samples(rate);
        for(int i=0;i<output.getNumSamples();++i){
            if(at>=end){for(int ch=0;ch<output.getNumChannels();++ch)output.clear(ch,i,output.getNumSamples()-i);playing=false;recording=false;break;}
            const auto step=song.stepAt(at,rate);if(step!=lastStep){trigger(drums,song,step);lastStep=step;}
            const float beat=drums.sample()*song.beatGain;
            for(int ch=0;ch<output.getNumChannels();++ch){
                float vocal=0;
                if(rec){vocal=output.getSample(ch,i);if(!std::isfinite(vocal))vocal=0;track.setSample(ch,(int)at,vocal);}
                else vocal=readTrack(ch,at/rate);
                output.setSample(ch,i,idw::guardedOutput(vocal*song.trackGain+beat,true));
            }
            if(rec&&output.getNumChannels()==1)track.setSample(1,(int)at,track.getSample(0,(int)at));
            ++at;if(rec)recorded=(int)at;
        }
        position=at;
    }
    juce::Result save(const juce::File& destination){
        const juce::ScopedLock guard(lock);if(playing)return juce::Result::fail("Stop playback or recording first.");
        juce::TemporaryFile temp(destination);auto out=temp.getFile().createOutputStream();if(!out)return juce::Result::fail("Cannot create project file.");
        out->writeInt(0x49445731);out->writeInt(1);out->writeInt(song.bpm);out->writeInt(song.bars);
        for(auto row:song.rows)out->writeInt(row);
        out->writeFloat(song.beatGain);out->writeFloat(song.trackGain);out->writeInt((int)takeRate);out->writeInt(recorded);
        for(int i=0;i<recorded;++i)for(int ch=0;ch<2;++ch)out->writeFloat(track.getSample(ch,i));
        out->flush();const bool ok=out->getStatus().wasOk();out.reset();
        return ok&&temp.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Project save failed; previous file preserved.");
    }
    juce::Result load(const juce::File& file){
        const juce::ScopedLock guard(lock);if(playing)return juce::Result::fail("Stop transport first.");
        auto in=file.createInputStream();if(!in||in->getTotalLength()<44||in->readInt()!=0x49445731||in->readInt()!=1)return juce::Result::fail("Not a supported IDW song project.");
        idw::SongPattern next;next.bpm=in->readInt();next.bars=in->readInt();
        for(auto& row:next.rows){const int value=in->readInt();if(value<0||value>65535)return juce::Result::fail("Invalid drum pattern.");row=(std::uint16_t)value;}
        next.beatGain=in->readFloat();next.trackGain=in->readFloat();const int sr=in->readInt(),frames=in->readInt();
        if(!next.valid()||sr<8000||sr>192000||frames<0||frames>sr*180||in->getTotalLength()!=44+(juce::int64)frames*8)return juce::Result::fail("Invalid or truncated song project.");
        juce::AudioBuffer<float> loaded(2,frames);
        for(int i=0;i<frames;++i)for(int ch=0;ch<2;++ch){const float value=in->readFloat();if(!std::isfinite(value))return juce::Result::fail("Invalid recorded audio.");loaded.setSample(ch,i,value);}
        track=std::move(loaded);song=next;takeRate=sr;recorded=frames;position=0;return juce::Result::ok();
    }
    juce::Result exportWav(const juce::File& file){
        const juce::ScopedLock guard(lock);if(playing)return juce::Result::fail("Stop transport first.");
        juce::TemporaryFile temp(file);std::unique_ptr<juce::OutputStream> stream=temp.getFile().createOutputStream();
        if(!stream)return juce::Result::fail("Cannot create WAV file.");
        juce::WavAudioFormat format;
        auto writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(24));
        if(!writer)return juce::Result::fail("Cannot initialize WAV writer.");
        idw::StudioSynth synth;synth.prepare(48000);configureDrums(synth);juce::AudioBuffer<float> block(2,1024);
        const auto end=song.samples(48000);std::int64_t previous=-1;
        for(std::int64_t base=0;base<end;base+=1024){const int count=(int)std::min<std::int64_t>(1024,end-base);
            for(int i=0;i<count;++i){const auto step=song.stepAt(base+i,48000);if(step!=previous){trigger(synth,song,step);previous=step;}
                const float beat=synth.sample()*song.beatGain;
                for(int ch=0;ch<2;++ch)block.setSample(ch,i,idw::guardedOutput(readTrack(ch,(base+i)/48000.0)*song.trackGain+beat,true));
            }
            if(!writer->writeFromAudioSampleBuffer(block,0,count))return juce::Result::fail("WAV write failed; check free disk space.");
        }
        writer.reset();return temp.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Could not finish WAV export.");
    }
private:
    static void configureDrums(idw::StudioSynth& synth){idw::StudioSynth::Settings settings;settings.gain=1;settings.delay=0;synth.configure(settings);}
    static void trigger(idw::StudioSynth& synth,const idw::SongPattern& pattern,std::int64_t step){const int notes[]{36,38,42};for(int row=0;row<3;++row)if(pattern.rows[(size_t)row]&(1u<<(step%16)))synth.message(0x99,notes[row],100);}
    float readTrack(int ch,double time)const{
        const double location=time*takeRate;const int index=(int)location;if(index<0||index>=recorded)return 0;
        const float first=track.getSample(ch,index),second=index+1<recorded?track.getSample(ch,index+1):0;
        return first+(second-first)*(float)(location-index);
    }
    mutable juce::CriticalSection lock;idw::SongPattern song;idw::StudioSynth drums;
    juce::AudioBuffer<float> track;int recorded=0;double rate=48000,takeRate=48000;std::int64_t lastStep=-1;
    std::atomic<double> displayRate{48000};
    std::atomic<bool> playing{false},recording{false};std::atomic<std::int64_t> position{0};
};
