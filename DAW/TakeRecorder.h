#pragma once
#include <JuceHeader.h>
#include <cmath>
// V14 mono/stereo WAV take recorder. Recording is opt-in and occurs via the
// device callback; samples are queued for disk writing on a background thread.
class IDWTakeRecorder final : public juce::AudioIODeviceCallback {
public:
    IDWTakeRecorder() : diskThread("IDW WAV disk writer") { diskThread.startThread(); }
    ~IDWTakeRecorder() override { stop(); diskThread.stopThread(5000); }
    bool start(const juce::File& destination, double sampleRate,
               int channelsToRecord=1, int firstInputChannel=0) {
        stop();
        if(!std::isfinite(sampleRate) || sampleRate<=0 || destination.existsAsFile() ||
           (channelsToRecord!=1 && channelsToRecord!=2) || firstInputChannel<0)return false;
        destination.getParentDirectory().createDirectory();
        std::unique_ptr<juce::FileOutputStream> output(destination.createOutputStream());
        if(!output)return false;
        juce::WavAudioFormat wav;
        auto writer=std::unique_ptr<juce::AudioFormatWriter>(
            wav.createWriterFor(output.get(),sampleRate,(unsigned int)channelsToRecord,24,{},0));
        if(!writer)return false;
        output.release(); // AudioFormatWriter now owns the stream.
        auto threaded=std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
            writer.release(),diskThread,32768);
        const juce::ScopedLock lock(guard);
        activeWriter=std::move(threaded);
        inputChannels=channelsToRecord;
        inputStartChannel=firstInputChannel;
        currentFile=destination;
        return true;
    }
    void stop(){
        const juce::ScopedLock lock(guard);
        activeWriter.reset();
    }
    bool isRecording() const {
        const juce::ScopedLock lock(guard);
        return activeWriter!=nullptr;
    }
    juce::File file() const { const juce::ScopedLock lock(guard);return currentFile; }
    void audioDeviceIOCallbackWithContext(const float* const* inputs,int inputsCount,
                                          float* const*,int,int samples,
                                          const juce::AudioIODeviceCallbackContext&) override {
        const juce::ScopedTryLock lock(guard);
        if(!lock.isLocked() || !activeWriter || samples<=0 || inputs==nullptr ||
           inputStartChannel>=inputsCount ||
           inputChannels>inputsCount-inputStartChannel)return;
        const float* selected[2] {inputs[inputStartChannel],nullptr};
        if(selected[0]==nullptr)return;
        if(inputChannels==2){
            selected[1]=inputs[inputStartChannel+1];
            if(selected[1]==nullptr)return;
        }
        activeWriter->write(selected,samples);
    }
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {stop();}
private:
    mutable juce::CriticalSection guard;
    juce::TimeSliceThread diskThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> activeWriter;
    juce::File currentFile;
    int inputChannels=1;
    int inputStartChannel=0;
};
