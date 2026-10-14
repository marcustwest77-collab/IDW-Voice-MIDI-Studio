#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

// Experimental V14 simultaneous recording engine. One device callback feeds up
// to eight independent mono WAV files. No allocation occurs in the callback.
// Not yet wired to the track-arm UI; see DAW/README.md.
class IDWMultiTakeRecorder final : public juce::AudioIODeviceCallback {
public:
    struct Input {
        juce::File file;
        int deviceChannel = 0; // zero-based physical input index
    };
    static constexpr std::size_t maxTracks = 8;

    IDWMultiTakeRecorder() : diskThread("IDW multitrack disk writer") {
        diskThread.startThread();
    }
    ~IDWMultiTakeRecorder() override {
        stop();
        diskThread.stopThread(5000);
    }

    // All-or-nothing validation before any file is created. A failed file
    // creation closes writers opened during this attempt.
    bool start(const std::vector<Input>& inputs, double sampleRate) {
        stop();
        if(!std::isfinite(sampleRate) || sampleRate <= 0 ||
           inputs.empty() || inputs.size() > maxTracks)return false;
        for(std::size_t i=0;i<inputs.size();++i){
            if(inputs[i].deviceChannel<0 || inputs[i].file.exists() ||
               !inputs[i].file.getParentDirectory().isDirectory())return false;
            for(std::size_t j=0;j<i;++j)
                if(inputs[i].file==inputs[j].file)return false;
        }

        std::array<Slot,maxTracks> pending;
        for(std::size_t i=0;i<inputs.size();++i){
            auto output=std::unique_ptr<juce::FileOutputStream>(
                inputs[i].file.createOutputStream());
            if(!output){
                cleanupPending(pending,inputs.size());
                return false;
            }
            juce::WavAudioFormat wav;
            auto writer=std::unique_ptr<juce::AudioFormatWriter>(
                wav.createWriterFor(output.get(),sampleRate,1,24,{},0));
            if(!writer){
                output.reset();
                inputs[i].file.deleteFile();
                cleanupPending(pending,inputs.size());
                return false;
            }
            output.release();
            pending[i].writer=std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
                writer.release(),diskThread,32768);
            pending[i].channel=inputs[i].deviceChannel;
            pending[i].file=inputs[i].file;
        }
        const juce::ScopedLock lock(guard);
        slots=std::move(pending);
        count=inputs.size();
        return true;
    }

    void stop(){
        const juce::ScopedLock lock(guard);
        for(auto& slot:slots)slot.writer.reset();
        count=0;
    }
    bool isRecording() const {
        const juce::ScopedLock lock(guard);
        return count!=0;
    }
    void audioDeviceIOCallbackWithContext(const float* const* inputs,int inputsCount,
                                          float* const*,int,int samples,
                                          const juce::AudioIODeviceCallbackContext&) override {
        const juce::ScopedTryLock lock(guard);
        if(!lock.isLocked() || count==0 || !inputs || samples<=0)return;
        // Refuse the whole block when any armed source is missing, so WAV
        // durations remain aligned. Device dropouts need separate diagnostics.
        for(std::size_t i=0;i<count;++i)
            if(slots[i].channel>=inputsCount || !inputs[slots[i].channel])return;
        for(std::size_t i=0;i<count;++i){
            const float* mono[] { inputs[slots[i].channel] };
            slots[i].writer->write(mono,samples);
        }
    }
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {stop();}

private:
    struct Slot {
        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
        juce::File file;
        int channel=0;
    };
    static void cleanupPending(std::array<Slot,maxTracks>& pending,std::size_t used) {
        for(std::size_t i=0;i<used;++i){
            pending[i].writer.reset();
            if(pending[i].file.existsAsFile())pending[i].file.deleteFile();
        }
    }
    mutable juce::CriticalSection guard;
    juce::TimeSliceThread diskThread;
    std::array<Slot,maxTracks> slots;
    std::size_t count=0;
};
