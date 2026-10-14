#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

// Experimental hardware loopback capture engine. The audio callback performs no
// allocation, file I/O, or blocking lock. The host must enable one physical input
// and output and stop/inspect capture on the message thread.
class IDWLoopbackCapture final : public juce::AudioIODeviceCallback {
public:
    static constexpr int maxSeconds=2;
    static constexpr int maxRate=192000;
    bool prepare(double rate,int input,int output) {
        // A completed or interrupted take must be acknowledged first.
        if(complete.load() || failed.load())return false;
        if(active.load() || !std::isfinite(rate) || rate<8000.0 || rate>maxRate ||
           input<0 || output<0)return false;
        sampleRate=rate;inputChannel=input;outputChannel=output;
        const int capacity=static_cast<int>(std::ceil(rate*0.8));
        captured.setSize(1,capacity,false,true,false);
        captured.clear();
        written.store(0);active.store(false);complete.store(false);failed.store(false);
        return true;
    }
    bool start() {
        if(active.load() || captured.getNumSamples()==0 ||
           complete.load() || failed.load())return false;
        written.store(0);complete.store(false);failed.store(false);active.store(true);
        return true;
    }
    void cancel(){active.store(false);complete.store(false);failed.store(false);}
    bool isActive()const{return active.load();}
    bool isComplete()const{return complete.load();}
    // The UI must consume each finished capture once to avoid repeat dialogs.
    bool consumeComplete(){return complete.exchange(false);}
    bool consumeFailed(){return failed.exchange(false);}
    int recordedSamples()const{return written.load();}
    double getSampleRate()const{return sampleRate;}
    const juce::AudioBuffer<float>& getCapturedBuffer()const{return captured;}
    void audioDeviceIOCallbackWithContext(const float* const* inputs,int inCount,
        float* const* outputs,int outCount,int frames,
        const juce::AudioIODeviceCallbackContext&) override {
        if(!active.load(std::memory_order_acquire) || frames<=0)return;
        if(!inputs || !outputs || inputChannel>=inCount || outputChannel>=outCount ||
           !inputs[inputChannel] || !outputs[outputChannel]){
            active.store(false);failed.store(true);return;
        }
        auto* output=outputs[outputChannel];
        const auto* input=inputs[inputChannel];
        const int startFrame=written.load(std::memory_order_relaxed);
        const int capacity=captured.getNumSamples();
        const int usable=juce::jmin(frames,capacity-startFrame);
        if(usable<=0){active.store(false);complete.store(true);return;}
        // Add to the existing output rather than clearing host playback.
        // When supported by the interface, route this output to a line input.
        const int pulses[]{100,200,300,400,500};
        for(const int ms:pulses){
            const int pulseFrame=static_cast<int>(std::llround(ms*sampleRate/1000.0));
            if(pulseFrame>=startFrame && pulseFrame<startFrame+usable){
                const int index=pulseFrame-startFrame;
                output[index]=juce::jlimit(-1.0f,1.0f,output[index]+0.25f);
            }
        }
        captured.copyFrom(0,startFrame,input,usable);
        written.store(startFrame+usable,std::memory_order_release);
        if(startFrame+usable==capacity){active.store(false);complete.store(true);}
    }
    void audioDeviceAboutToStart(juce::AudioIODevice* device)override{
        // A hardware device restart invalidates the sample clock and routing
        // assumed when the calibration capture was prepared.
        if(active.load() && (!device ||
            std::abs(device->getCurrentSampleRate()-sampleRate)>0.01)){
            active.store(false);
            failed.store(true);
        }
    }
    void audioDeviceStopped()override{
        if(active.exchange(false))failed.store(true);
    }
private:
    juce::AudioBuffer<float> captured;
    std::atomic<bool> active{false},complete{false},failed{false};
    std::atomic<int> written{0};
    double sampleRate=0;
    int inputChannel=0,outputChannel=0;
};
