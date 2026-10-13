#pragma once
#include "SessionModel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace idw::daw {
// Five lightweight oscillator timbres. MIDI note timing stays unchanged.
enum class SynthPreset { keys, bass, lead, organ, pad };
inline float synthWave(SynthPreset preset,double phase,double fundamentalHz,double sampleRate){
    // Exclude overtones at or above Nyquist to avoid their folding into
    // unrelated lower pitches on devices with lower sample rates.
    const double nyquist=sampleRate*0.5;
    const float fundamental=(float)std::sin(phase);
    const bool second=fundamentalHz<nyquist/2.0;
    const bool third=fundamentalHz<nyquist/3.0;
    if(preset==SynthPreset::bass)
        return (second?0.8f:1.0f)*fundamental+
               (second?0.2f*(float)std::sin(2.0*phase):0.0f);
    if(preset==SynthPreset::organ)
        return (second?0.75f:1.0f)*fundamental+
               (second?0.25f*(float)std::sin(2.0*phase):0.0f);
    if(preset==SynthPreset::pad)
        return (second?0.9f:1.0f)*fundamental+
               (second?0.1f*(float)std::sin(2.0*phase):0.0f);
    if(preset==SynthPreset::lead)
        return (third?0.6f:(second?0.75f:1.0f))*fundamental+
               (second?(third?0.3f:0.25f)*(float)std::sin(2.0*phase):0.0f)+
               (third?0.1f*(float)std::sin(3.0*phase):0.0f);
    return fundamental;
}

// Stateless prototype sine synthesizer: renders notes by absolute timeline sample
// position so output is deterministic across block sizes and does not allocate.
// Call only when the session is not being modified by the UI.
inline void renderMidiSine(const Session& s,float* left,float* right,int samples,
                           std::int64_t startFrame,double sampleRate) {
    if(!left || samples<=0 || sampleRate<=0 || !std::isfinite(sampleRate) ||
       !std::isfinite(s.bpm) || s.bpm<=0 || startFrame<0)return;
    // Guard invalid tempo before calculating frame positions; malformed projects
    // should never feed nonfinite values to the audio callback.
    const double framesPerBeat=sampleRate*60.0/s.bpm;
    if(!std::isfinite(framesPerBeat) || framesPerBeat<=0)return;
    bool anySolo=false;
    for(const auto& t:s.tracks)if(t.solo)anySolo=true;
    // Normalize active MIDI voices so stacked notes do not overwhelm the mix.
    // Count only notes overlapping the current buffer; no allocations.
    std::size_t activeVoices=0;
    const double bufferEndBeat=(double)(startFrame+samples)/framesPerBeat;
    const double bufferStartBeat=(double)startFrame/framesPerBeat;
    for(const auto& track:s.tracks){
        if(track.type!=TrackType::midi || track.mute || (anySolo && !track.solo))continue;
        for(const auto& note:track.notes)
            if(std::isfinite(note.startBeat) && std::isfinite(note.lengthBeats) &&
               note.lengthBeats>0 && note.startBeat<bufferEndBeat &&
               note.startBeat+note.lengthBeats>bufferStartBeat)++activeVoices;
    }
    const float voiceScale=1.0f/std::sqrt((float)std::max<std::size_t>(1,activeVoices));
    for(const auto& t:s.tracks){
        if(t.type!=TrackType::midi || t.mute || (anySolo && !t.solo))continue;
        const float gain=std::clamp(t.gain,0.0f,2.0f)*0.12f*voiceScale;
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
            // Compare absolute frame ranges first to avoid subtraction overflow
            // for exceptionally distant notes in malformed sessions.
            const auto blockEnd=(double)startFrame+(double)samples;
            if((double)noteEnd<=(double)startFrame || (double)noteStart>=blockEnd)continue;
            const auto first=(std::int64_t)std::max(0.0,(double)noteStart-(double)startFrame);
            const auto last=(std::int64_t)std::min((double)samples,(double)noteEnd-(double)startFrame);
            if(first>=last)continue;
            // Lookup immutable equal-tempered MIDI pitch frequencies instead of
            // evaluating pow() separately for each rendered note and buffer.
            static const auto pitchHz=[]{
                std::array<double,128> frequencies{};
                for(int pitch=0;pitch<128;++pitch)
                    frequencies[(std::size_t)pitch]=440.0*std::pow(2.0,(pitch-69)/12.0);
                return frequencies;
            }();
            const double freq=pitchHz[(std::size_t)n.pitch];
            // Skip inaudible / aliased notes rather than generating frequencies
            // above the Nyquist limit (particularly on low sample-rate devices).
            if(!std::isfinite(freq) || freq>=sampleRate*0.5)continue;
            const double increment=6.283185307179586*freq/sampleRate;
            const float velocity=n.velocity/127.0f;
            // Calculate per-note constants outside the sample loop.
            const auto total=noteEnd-noteStart;
            const auto attackFrames=std::max<std::int64_t>(1,(std::int64_t)(sampleRate*t.synthAttackMs/1000.0));
            const auto releaseFrames=std::max<std::int64_t>(1,(std::int64_t)(sampleRate*t.synthReleaseMs/1000.0));
            const auto preset=static_cast<SynthPreset>(std::clamp(t.synthPreset,0,4));
            // Resolve harmonic eligibility once per note rather than per sample.
            const double nyquist=sampleRate*0.5;
            const bool second=freq<nyquist/2.0;
            const bool third=freq<nyquist/3.0;
            const float fundamentalGain=preset==SynthPreset::pad?(second?0.9f:1.0f):
                preset==SynthPreset::organ?(second?0.75f:1.0f):
                preset==SynthPreset::bass?(second?0.8f:1.0f):
                preset==SynthPreset::lead?(third?0.6f:(second?0.75f:1.0f)):1.0f;
            const float secondGain=preset==SynthPreset::pad?(second?0.1f:0.0f):
                preset==SynthPreset::organ?(second?0.25f:0.0f):
                preset==SynthPreset::bass?(second?0.2f:0.0f):
                preset==SynthPreset::lead&&second?(third?0.3f:0.25f):0.0f;
            const float thirdGain=preset==SynthPreset::lead&&third?0.1f:0.0f;
            for(auto i=first;i<last;++i){
                const auto offset=(startFrame+i-noteStart);
                const float fade=std::clamp(std::min((double)offset/attackFrames,
                                         (double)(total-1-offset)/releaseFrames),0.0,1.0);
                const double phase=(double)offset*increment;
                float tone=fundamentalGain*(float)std::sin(phase);
                if(secondGain>0.0f)tone+=secondGain*(float)std::sin(2.0*phase);
                if(thirdGain>0.0f)tone+=thirdGain*(float)std::sin(3.0*phase);
                const float wave=tone*velocity*std::max(0.0f,fade);
                left[i]+=wave*l;
                if(right)right[i]+=wave*r;
            }
        }
    }
}
}
