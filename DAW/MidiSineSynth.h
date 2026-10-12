#pragma once
#include "SessionModel.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace idw::daw {
// Three lightweight oscillator timbres. MIDI note timing stays unchanged.
enum class SynthPreset { keys, bass, lead };
inline float synthWave(SynthPreset preset,double phase){
    const float fundamental=(float)std::sin(phase);
    if(preset==SynthPreset::bass)return 0.8f*fundamental+0.2f*(float)std::sin(phase*2.0);
    if(preset==SynthPreset::lead)return 0.6f*fundamental+0.3f*(float)std::sin(phase*2.0)+0.1f*(float)std::sin(phase*3.0);
    return fundamental;
}

// Stateless prototype sine synthesizer: renders notes by absolute timeline sample
// position so output is deterministic across block sizes and does not allocate.
// Call only when the session is not being modified by the UI.
inline void renderMidiSine(const Session& s,float* left,float* right,int samples,
                           std::int64_t startFrame,double sampleRate,SynthPreset preset=SynthPreset::keys) {
    if(!left || !right || samples<=0 || sampleRate<=0 || !std::isfinite(sampleRate))return;
    const double framesPerBeat=sampleRate*60.0/s.bpm;
    if(!std::isfinite(framesPerBeat) || framesPerBeat<=0)return;
    bool anySolo=false;
    for(const auto& t:s.tracks)if(t.solo)anySolo=true;
    for(const auto& t:s.tracks){
        if(t.type!=TrackType::midi || t.mute || (anySolo && !t.solo))continue;
        const float gain=std::clamp(t.gain,0.0f,2.0f)*0.12f;
        const float l=gain*(t.pan>0.0f?1.0f-t.pan:1.0f);
        const float r=gain*(t.pan<0.0f?1.0f+t.pan:1.0f);
        for(const auto& n:t.notes){
            if(n.pitch<0 || n.pitch>127 || n.velocity<1 || n.velocity>127 ||
               !std::isfinite(n.startBeat) || !std::isfinite(n.lengthBeats) ||
               n.startBeat<0 || n.lengthBeats<=0)continue;
            const double noteStartDouble=n.startBeat*framesPerBeat;
            const double noteEndDouble=(n.startBeat+n.lengthBeats)*framesPerBeat;
            if(!std::isfinite(noteStartDouble) || !std::isfinite(noteEndDouble) ||
               noteStartDouble<0 || noteEndDouble>9e15)continue;
            const auto noteStart=(std::int64_t)std::llround(noteStartDouble);
            const auto noteEnd=(std::int64_t)std::llround(noteEndDouble);
            const auto first=std::max<std::int64_t>(0,noteStart-startFrame);
            const auto last=std::min<std::int64_t>(samples,noteEnd-startFrame);
            if(first>=last)continue;
            const double freq=440.0*std::pow(2.0,(n.pitch-69)/12.0);
            const double increment=6.283185307179586*freq/sampleRate;
            const float velocity=n.velocity/127.0f;
            for(auto i=first;i<last;++i){
                const auto offset=(startFrame+i-noteStart);
                const auto total=noteEnd-noteStart;
                const float fade=(float)std::min<std::int64_t>(std::min(offset,total-1-offset),
                                         (std::int64_t)(sampleRate*0.005)) /
                                 (float)std::max<std::int64_t>(1,(std::int64_t)(sampleRate*0.005));
                const float wave=synthWave(preset,(double)offset*increment)*velocity*std::max(0.0f,fade);
                left[i]+=wave*l;
                if(right)right[i]+=wave*r;
            }
        }
    }
}
}
