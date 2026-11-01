#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <algorithm>

// AudioDeviceManager sums separately registered callbacks. Capture engines can
// return early or add monitoring/pulses, but must never inherit stale scratch
// audio from another callback/block. The manager performs the final mix.
class IDWIsolatedAudioCallback final : public juce::AudioIODeviceCallback {
public:
    explicit IDWIsolatedAudioCallback(juce::AudioIODeviceCallback& value):callback(value) {}
    void audioDeviceIOCallbackWithContext(const float* const* inputs,int inputCount,
        float* const* outputs,int outputCount,int frames,
        const juce::AudioIODeviceCallbackContext& context) override {
        if(outputs && frames>0)
            for(int channel=0;channel<outputCount;++channel)
                if(outputs[channel])std::fill_n(outputs[channel],frames,0.0f);
        callback.audioDeviceIOCallbackWithContext(inputs,inputCount,outputs,outputCount,frames,context);
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {
        callback.audioDeviceAboutToStart(device);
    }
    void audioDeviceStopped() override {callback.audioDeviceStopped();}
    void audioDeviceError(const juce::String& error) override {callback.audioDeviceError(error);}
private:
    juce::AudioIODeviceCallback& callback;
};
