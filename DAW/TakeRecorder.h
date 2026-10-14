#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <atomic>
#include <cstdint>
// V14 mono/stereo WAV take recorder. Recording is opt-in and occurs via the
// device callback; samples are queued for disk writing on a background thread.
class IDWTakeRecorder final : public juce::AudioIODeviceCallback {
public:
    IDWTakeRecorder() : diskThread("IDW WAV disk writer") { diskThread.startThread(); }
    ~IDWTakeRecorder() override { stop(); diskThread.stopThread(5000); }
    bool start(const juce::File& destination, double sampleRate,
               int channelsToRecord=1, int firstInputChannel=0) {
        stop();
        if(!std::isfinite(sampleRate) || sampleRate<8000.0 || sampleRate>192000.0 ||
           destination.existsAsFile() ||
           (channelsToRecord!=1 && channelsToRecord!=2) || firstInputChannel<0)return false;
        if(!destination.getParentDirectory().createDirectory())return false;
        std::unique_ptr<juce::FileOutputStream> output(destination.createOutputStream());
        if(!output){
            // File creation may leave an empty or partial destination behind.
            if(destination.existsAsFile())destination.deleteFile();
            return false;
        }
        juce::WavAudioFormat wav;
        auto writer=std::unique_ptr<juce::AudioFormatWriter>(
            wav.createWriterFor(output.get(),sampleRate,(unsigned int)channelsToRecord,24,{},0));
        if(!writer){
            output.reset();
            destination.deleteFile();
            return false;
        }
        output.release(); // AudioFormatWriter now owns the stream.
        auto threaded=std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
            writer.release(),diskThread,32768);
        const juce::ScopedLock lock(guard);
        activeWriter=std::move(threaded);
        // Reset diagnostics only after the next take actually starts.
        droppedBlocks.store(0,std::memory_order_relaxed);
        rejectedWrites.store(0,std::memory_order_relaxed);
        capturedFrames.store(0,std::memory_order_relaxed);
        queuedFrames.store(0,std::memory_order_relaxed);
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
    std::uint64_t getDroppedBlocks() const noexcept {return droppedBlocks.load(std::memory_order_relaxed);}
    std::uint64_t getRejectedWrites() const noexcept {return rejectedWrites.load(std::memory_order_relaxed);}
    // Callback-received frames provide an audit trail for take duration. This
    // does not by itself establish synchronization with the playback clock.
    std::uint64_t getCapturedFrames() const noexcept {return capturedFrames.load(std::memory_order_relaxed);}
    std::uint64_t getQueuedFrames() const noexcept {return queuedFrames.load(std::memory_order_relaxed);}
    void audioDeviceIOCallbackWithContext(const float* const* inputs,int inputsCount,
                                          float* const*,int,int samples,
                                          const juce::AudioIODeviceCallbackContext&) override {
        const juce::ScopedTryLock lock(guard);
        if(!lock.isLocked()){
            droppedBlocks.fetch_add(1,std::memory_order_relaxed);
            return;
        }
        if(!activeWriter || samples<=0)return;
        if(inputs==nullptr || inputStartChannel<0 || inputStartChannel>=inputsCount ||
           inputChannels>inputsCount-inputStartChannel){
            droppedBlocks.fetch_add(1,std::memory_order_relaxed);
            return;
        }
        const float* selected[2] {inputs[inputStartChannel],nullptr};
        if(selected[0]==nullptr){droppedBlocks.fetch_add(1,std::memory_order_relaxed);return;}
        if(inputChannels==2){
            selected[1]=inputs[inputStartChannel+1];
            if(selected[1]==nullptr){droppedBlocks.fetch_add(1,std::memory_order_relaxed);return;}
        }
        // Count received audio separately from frames accepted by the WAV queue.
        capturedFrames.fetch_add(static_cast<std::uint64_t>(samples),std::memory_order_relaxed);
        if(!activeWriter->write(selected,samples))
            rejectedWrites.fetch_add(1,std::memory_order_relaxed);
        else
            queuedFrames.fetch_add(static_cast<std::uint64_t>(samples),std::memory_order_relaxed);
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
    std::atomic<std::uint64_t> droppedBlocks{0},rejectedWrites{0},capturedFrames{0},queuedFrames{0};
};
