#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "SessionModel.h"
#include "ClipEnvelope.h"
#include "MidiSineSynth.h"
#include <algorithm>
#include <cmath>
#include <atomic>
#include <functional>
namespace idw::daw {
// An export-owned processor, prepared for 1024-frame blocks at the export rate.
// The caller owns its lifetime; the live playback instance must never be used.
struct OfflineBounceEffect {
    size_t track=0;
    std::string preset;
    int latency=0,tail=0;
    std::function<bool(juce::AudioBuffer<float>&)> process;
};
// Offline stereo bounce. Reads source files into small blocks and sums against
// the shared beat clock, including the built-in MIDI instruments.
// Call only from a non-audio thread. Returns false and a readable error.
inline bool renderSessionWav(const Session& session,const juce::File& destination,
                            juce::String& error,int sampleRate=44100,float masterGain=1.0f,float ceilingDb=-1.0f,float lowDb=0.0f,float highDb=0.0f,
                            std::atomic<float>* progress=nullptr,std::atomic<bool>* cancel=nullptr,
                            const OfflineBounceEffect* effect=nullptr){
    error.clear();
    size_t savedEffects=0;
    for(size_t i=0;i<session.tracks.size();++i)if(!session.tracks[i].effectPreset.empty()){
        ++savedEffects;
        if(!effect || session.tracks[i].type!=TrackType::audio || effect->track!=i || effect->preset!=session.tracks[i].effectPreset){
            error="Saved track effect must be prepared before WAV export.";return false;
        }
    }
    if(savedEffects>1 || (effect && (savedEffects!=1 || !effect->process ||
        effect->latency<0 || effect->latency>sampleRate*30LL || effect->tail<0 || effect->tail>sampleRate*30LL))){
        error="Invalid offline effect configuration.";return false;
    }
    if(!std::isfinite(session.bpm) || session.bpm<=0 || sampleRate<8000 || sampleRate>192000 || !std::isfinite(masterGain) || !std::isfinite(ceilingDb) || !std::isfinite(lowDb) || !std::isfinite(highDb)){
        error="Invalid bounce rate or tempo.";return false;
    }
    double lastBeat=0.0;
    for(const auto& t:session.tracks){
        if(!std::isfinite(t.gain) || !std::isfinite(t.pan)){
            error="Invalid track mix.";return false;
        }
        if(t.type==TrackType::midi){
            if(!std::isfinite(t.synthAttackMs) || !std::isfinite(t.synthReleaseMs) ||
               t.synthAttackMs<0 || t.synthAttackMs>2000 ||
               t.synthReleaseMs<0 || t.synthReleaseMs>2000){
                error="Invalid MIDI instrument envelope.";return false;
            }
            for(const auto& n:t.notes){
                if(n.pitch<0 || n.pitch>127 || n.velocity<1 || n.velocity>127 ||
                   !std::isfinite(n.startBeat) || !std::isfinite(n.lengthBeats) ||
                   n.startBeat<0 || n.lengthBeats<=0 ||
                   !std::isfinite(n.startBeat+n.lengthBeats)){
                    error="Invalid MIDI note.";return false;
                }
                lastBeat=std::max(lastBeat,n.startBeat+n.lengthBeats);
            }
        }
        for(const auto& c:t.clips){
            if(!ClipEnvelope(c,session.bpm,sampleRate).valid ||
               !std::isfinite(c.sourceOffsetSeconds) || c.sourceOffsetSeconds<0){
                error="Invalid clip timing or fade.";return false;
            }
            lastBeat=std::max(lastBeat,c.startBeat+c.lengthBeats);
        }
    }
    if(lastBeat<=0){error="Session has no audio clips or MIDI notes.";return false;}
    const double requestedFrames=std::ceil(lastBeat*60.0/session.bpm*sampleRate);
    if(!std::isfinite(requestedFrames) || requestedFrames<=0 || requestedFrames>(double)sampleRate*60*60*4){
        error="Export duration must be under four hours.";return false;
    }
    const bool anySolo=std::any_of(session.tracks.begin(),session.tracks.end(),
        [](const Track& t){return t.solo;});
    const bool audibleEffect=effect && !session.tracks[effect->track].mute &&
        (!anySolo || session.tracks[effect->track].solo);
    const int latency=audibleEffect?effect->latency:0;
    const int64_t frames=(int64_t)requestedFrames+(audibleEffect?effect->tail:0)+latency;
    if(destination.existsAsFile()){error="Choose a new WAV filename.";return false;}
    auto stream=std::unique_ptr<juce::FileOutputStream>(destination.createOutputStream());
    if(!stream){error="Cannot create WAV output.";return false;}
    juce::WavAudioFormat wav;
    auto writer=std::unique_ptr<juce::AudioFormatWriter>(
        wav.createWriterFor(stream.get(),(double)sampleRate,2,24,{},0));
    if(!writer){error="Cannot create WAV writer.";destination.deleteFile();return false;}
    stream.release();
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    constexpr int block=1024;
    juce::AudioBuffer<float> mix(2,block),scratch(2,block),wet(2,block),delay(2,std::max(1,latency));
    delay.clear();int delayPosition=0;
    // Open sources once, before rendering.
    struct ActiveClip {
        std::unique_ptr<juce::AudioFormatReader> reader;
        int64_t start=0,end=0,offset=0;
        double sourceRate=44100.0;
        float left=1,right=1;
        ClipEnvelope envelope;
        bool effected=false;
    };
    std::vector<ActiveClip> clips;
    for(const auto& track:session.tracks){
        if(track.type!=TrackType::audio || track.mute || (anySolo&&!track.solo))continue;
        for(const auto& clip:track.clips){
            auto reader=std::unique_ptr<juce::AudioFormatReader>(
                formats.createReaderFor(juce::File(juce::String(clip.sourcePath))));
            if(!reader){error="Missing or unsupported audio: "+juce::String(clip.sourcePath);writer.reset();destination.deleteFile();return false;}

            const ClipEnvelope envelope(clip,session.bpm,sampleRate);
            const int64_t start=envelope.start,length=envelope.length;
            const float pan=std::clamp(track.pan,-1.0f,1.0f),gain=std::clamp(track.gain,0.0f,2.0f);
            const double sourceRate=reader->sampleRate;
            int64_t sourceOffset=0;
            if(!std::isfinite(sourceRate) || sourceRate<=0 ||
               !roundedSampleFrame(clip.sourceOffsetSeconds*sourceRate,sourceOffset)){
                error="Invalid source timing.";writer.reset();destination.deleteFile();return false;
            }
            clips.push_back({std::move(reader),start,start+length,sourceOffset,sourceRate,
                gain*(pan>0?1-pan:1),gain*(pan<0?1+pan:1),envelope,
                audibleEffect && &track==&session.tracks[effect->track]});
        }
    }
    const float outputCeiling=std::pow(10.0f,std::clamp(ceilingDb,-12.0f,-0.1f)/20.0f);
    const float outputGain=std::clamp(masterGain,0.0f,1.5f);
    const float alpha=(float)std::exp(-2.0*juce::MathConstants<double>::pi*500.0/sampleRate);
    const float bassGain=std::pow(10.0f,std::clamp(lowDb,-12.0f,12.0f)/20.0f);
    const float trebleGain=std::pow(10.0f,std::clamp(highDb,-12.0f,12.0f)/20.0f);
    float filterState[2]{0.0f,0.0f};
    if(progress)progress->store(0.0f,std::memory_order_relaxed);
    for(int64_t pos=0;pos<frames;pos+=block){
        if(cancel && cancel->load(std::memory_order_relaxed)){
            error="Export canceled.";writer.reset();destination.deleteFile();return false;
        }
        const int count=(int)std::min<int64_t>(block,frames-pos);
        mix.clear();
        wet.clear();
        for(auto& clip:clips){
            const int64_t begin=std::max(pos,clip.start),end=std::min(pos+count,clip.end);
            if(end<=begin)continue;
            const int offset=(int)(begin-pos),number=(int)(end-begin);
            // Linear interpolation at the source sample rate, with one extra frame
            // to interpolate across export block boundaries.
            const double ratio=clip.sourceRate/sampleRate;
            if(!std::isfinite(ratio) || ratio<=0.0 || ratio>1000.0){
                error="Unsupported source sample rate.";
                writer.reset();destination.deleteFile();return false;
            }
            const double first=clip.offset+(begin-clip.start)*ratio;
            int64_t firstIndex=0;
            if(!roundedSampleFrame(std::floor(first),firstIndex) ||
               firstIndex>std::numeric_limits<int64_t>::max()-1000000){
                error="Invalid source sample position.";
                writer.reset();destination.deleteFile();return false;
            }
            const int64_t sourceStart=firstIndex-(ratio>1.0?12:0);
            // Validate in floating point before narrowing to int: malformed
            // source metadata must never trigger an out-of-range conversion.
            const double requestedCount=std::ceil(first-sourceStart+(number-1)*ratio)
                                        +2.0+(ratio>1.0?12.0:0.0);
            if(!std::isfinite(first) || !std::isfinite(requestedCount) ||
               requestedCount<2.0 || requestedCount>1000000.0){
                error="Invalid resampling buffer size.";
                writer.reset();destination.deleteFile();return false;
            }
            const int sourceCount=(int)requestedCount;
            // Reuse the scratch buffer across clips and blocks rather than allocating
            // a temporary AudioBuffer for every overlap.
            if(scratch.getNumSamples()<sourceCount)
                scratch.setSize(2,sourceCount,false,false,true);
            scratch.clear(0,sourceCount);
            // Readers may not safely accept negative source positions.
            const int64_t safeStart=std::max<int64_t>(0,sourceStart);
            const int leading=(int)(safeStart-sourceStart);
            if(leading<sourceCount)
                clip.reader->read(&scratch,leading,sourceCount-leading,safeStart,true,true);
            for(int i=0;i<number;++i){
                const double at=first-sourceStart+i*ratio;
                const int index=(int)std::floor(at);
                const float fraction=(float)(at-index);
                // A short windowed-sinc kernel suppresses out-of-band energy
                // during downsampling; linear interpolation remains for upsampling.
                const int right=clip.reader->numChannels>1?1:0;
                float l=0.0f,r=0.0f;
                if(ratio<=1.0){
                    l=scratch.getSample(0,index)*(1.0f-fraction)+scratch.getSample(0,index+1)*fraction;
                    r=scratch.getSample(right,index)*(1.0f-fraction)+scratch.getSample(right,index+1)*fraction;
                }else{
                    // Compute sinc/window coefficients once and use the same
                    // kernel for both channels, avoiding duplicate trig work.
                    constexpr int radius=12;
                    const double cutoff=0.5/ratio;
                    double leftValue=0.0,rightValue=0.0,weight=0.0;
                    for(int tap=-radius+1;tap<=radius;++tap){
                        const int position=index+tap;
                        if(position<0 || position>=sourceCount)continue;
                        const double distance=at-position;
                        if(std::abs(distance)>=radius)continue;
                        const double sincArg=2.0*cutoff*distance;
                        const double sinc=std::abs(sincArg)<1.0e-10?1.0:
                            std::sin(juce::MathConstants<double>::pi*sincArg)/
                            (juce::MathConstants<double>::pi*sincArg);
                        const double window=0.5+0.5*std::cos(
                            juce::MathConstants<double>::pi*distance/radius);
                        const double coefficient=2.0*cutoff*sinc*window;
                        leftValue+=coefficient*scratch.getSample(0,position);
                        rightValue+=coefficient*scratch.getSample(right,position);
                        weight+=coefficient;
                    }
                    if(std::abs(weight)>1.0e-10){
                        l=(float)(leftValue/weight);
                        r=(float)(rightValue/weight);
                    }
                }
                const float envelopeGain=clip.envelope.gain(begin+i);
                auto& bus=clip.effected?wet:mix;
                bus.addSample(0,offset+i,l*(clip.effected?1.0f:clip.left)*envelopeGain);
                bus.addSample(1,offset+i,r*(clip.effected?1.0f:clip.right)*envelopeGain);
            }
        }
        renderMidiSine(session,mix.getWritePointer(0),mix.getWritePointer(1),count,pos,sampleRate);
        if(audibleEffect){
            float* pointers[]{wet.getWritePointer(0),wet.getWritePointer(1)};
            juce::AudioBuffer<float> view(pointers,2,count);
            bool processed=false;
            try{processed=effect->process(view);}catch(...){processed=false;}
            if(processed)for(int ch=0;ch<2;++ch)for(int i=0;i<count;++i)
                if(!std::isfinite(wet.getSample(ch,i)))processed=false;
            if(!processed){error="Track effect failed during WAV export.";writer.reset();destination.deleteFile();return false;}
            const auto& track=session.tracks[effect->track];
            const float pan=std::clamp(track.pan,-1.0f,1.0f),gain=std::clamp(track.gain,0.0f,2.0f);
            const float levels[]{gain*(pan>0?1-pan:1),gain*(pan<0?1+pan:1)};
            // Delay dry tracks to match the plugin, then trim the shared delay
            // at the writer. This preserves alignment, including MIDI tracks.
            for(int i=0;i<count;++i){
                for(int ch=0;ch<2;++ch){
                    float dry=mix.getSample(ch,i);
                    if(latency){const float previous=delay.getSample(ch,delayPosition);delay.setSample(ch,delayPosition,dry);dry=previous;}
                    mix.setSample(ch,i,dry+wet.getSample(ch,i)*levels[ch]);
                }
                if(latency)delayPosition=(delayPosition+1)%latency;
            }
        }
        const int skip=(int)std::clamp<int64_t>(latency-pos,0,count);
        for(int ch=0;ch<2;++ch){
            auto* p=mix.getWritePointer(ch);
            float state=filterState[ch];
            for(int i=skip;i<count;++i){
                const float input=p[i];
                state=alpha*state+(1.0f-alpha)*input;
                const float shaped=bassGain*state+trebleGain*(input-state);
                p[i]=std::clamp(shaped*outputGain,-outputCeiling,outputCeiling);
            }
            filterState[ch]=state;
        }
        const float* channels[2]={mix.getReadPointer(0,skip),mix.getReadPointer(1,skip)};
        if(count>skip && !writer->writeFromFloatArrays(channels,2,count-skip)){
            error="Failed while writing WAV samples.";writer.reset();destination.deleteFile();return false;
        }
        if(progress)progress->store((float)(pos+count)/(float)frames,std::memory_order_relaxed);
    }
    writer.reset();return true;
}
// Project export uses the same recalled master settings as live playback.
// The lower-level renderer keeps explicit processing arguments for callers
// that deliberately render a reference mix with different master settings.
inline bool renderProjectWav(const Session& session,const juce::File& destination,
                             juce::String& error,int sampleRate=44100,
                             std::atomic<float>* progress=nullptr,std::atomic<bool>* cancel=nullptr,
                             const OfflineBounceEffect* effect=nullptr){
    if(!validMasterMix(session.masterMix)){error="Invalid project master mix.";return false;}
    const auto& m=session.masterMix;
    return renderSessionWav(session,destination,error,sampleRate,(float)m.gain,
                           (float)m.ceilingDb,(float)m.lowDb,(float)m.highDb,progress,cancel,effect);
}
}
