#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "ClipEnvelope.h"
namespace idw::daw {
// Called only while the session transport is playing. The file reader/transport
// and scratch capacity are prepared beforehand; this function does not resize
// buffers. Shared by the workstation callback and device-free integration tests.
inline ClipWindow readClipWindow(juce::AudioTransportSource& transport,
                                 juce::AudioBuffer<float>& scratch,juce::int64& nextSourceFrame,
                                 const Clip& clip,double bpm,double rate,
                                 int64_t blockStart,int blockFrames) {
    const auto window=overlap(blockStart,blockFrames,rate,bpm,
        clip.startBeat,clip.lengthBeats,clip.sourceOffsetSeconds);
    if(window.frames<=0 || window.frames>scratch.getNumSamples())return {};
    const ClipEnvelope envelope(clip,bpm,rate);
    if(!envelope.valid)return {};
    scratch.clear(0,window.frames);
    // Capture EOF before seeking, since the seek changes hasStreamFinished().
    // An explicit user stop must never be mistaken for an EOF restart.
    const bool restartAfterEOF=!transport.isPlaying() && transport.hasStreamFinished();
    if(nextSourceFrame!=window.sourceFrame)
        transport.setNextReadPosition(window.sourceFrame);
    // EOF stops AudioTransportSource. Seeking alone does not restart it.
    // Do not flush a continuous cursor here: its resampler may still hold the
    // tail that was prefetched when the underlying reader reached EOF.
    if(restartAfterEOF)transport.start();
    nextSourceFrame=window.sourceFrame+window.frames;
    juce::AudioSourceChannelInfo readInfo(&scratch,0,window.frames);
    transport.getNextAudioBlock(readInfo);
    for(int ch=0;ch<scratch.getNumChannels();++ch)
        envelope.apply(scratch.getWritePointer(ch),window.frames,blockStart+window.offset);
    return window;
}
}
