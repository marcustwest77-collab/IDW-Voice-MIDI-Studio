#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

// Experimental V14 simultaneous recording engine. One device callback feeds up
// to eight independent mono WAV files. No allocation occurs in the callback.
// Connected to the experimental arm/input UI; physical-device QA is pending.
class IDWMultiTakeRecorder final : public juce::AudioIODeviceCallback {
public:
    struct Input {
        juce::File file;
        int deviceChannel = 0; // zero-based packed audio callback input slot
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
        droppedBlocks.store(0,std::memory_order_relaxed);
        rejectedWrites.store(0,std::memory_order_relaxed);
        for(auto& peak:inputPeaks)peak.store(0.0f,std::memory_order_relaxed);
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
        monitorGain.store(0.0f,std::memory_order_relaxed);
    }
    // Monitoring is disabled by default. Only call this after the user has
    // confirmed headphone use; speakers can produce acoustic feedback.
    void setHeadphoneMonitor(bool enabled,float gain=0.1f) noexcept {
        monitorGain.store(enabled?juce::jlimit(0.0f,0.25f,gain):0.0f,
                          std::memory_order_relaxed);
    }
    std::uint64_t getDroppedBlocks() const noexcept {
        return droppedBlocks.load(std::memory_order_relaxed);
    }
    float getInputPeak(std::size_t slot) const noexcept {
        return slot<maxTracks ? inputPeaks[slot].load(std::memory_order_relaxed) : 0.0f;
    }
    std::uint64_t getRejectedWrites() const noexcept {
        return rejectedWrites.load(std::memory_order_relaxed);
    }
    bool isRecording() const {
        const juce::ScopedLock lock(guard);
        return count!=0;
    }
    void audioDeviceIOCallbackWithContext(const float* const* inputs,int inputsCount,
                                          float* const* outputs,int outputsCount,int samples,
                                          const juce::AudioIODeviceCallbackContext&) override {
        const juce::ScopedTryLock lock(guard);
        if(!lock.isLocked()){
            droppedBlocks.fetch_add(1,std::memory_order_relaxed);
            return;
        }
        if(count==0 || samples<=0)return;
        if(!inputs){droppedBlocks.fetch_add(1,std::memory_order_relaxed);return;}
        // Refuse the whole block when any armed source is missing, so WAV
        // durations remain aligned. Device dropouts need separate diagnostics.
        for(std::size_t i=0;i<count;++i)
            if(slots[i].channel>=inputsCount || !inputs[slots[i].channel]){
                droppedBlocks.fetch_add(1,std::memory_order_relaxed);
                return;
            }
        const float monitor=monitorGain.load(std::memory_order_relaxed);
        for(std::size_t i=0;i<count;++i){
            const float* source=inputs[slots[i].channel];
            float peak=0.0f;
            for(int frame=0;frame<samples;++frame){
                const float value=source[frame];
                if(std::isfinite(value))peak=std::max(peak,std::abs(value));
            }
            inputPeaks[i].store(peak,std::memory_order_relaxed);
            // Limited direct monitoring of the FIRST input only; dry signal,
            // no allocations, and never active unless explicitly enabled.
            if(i==0 && monitor>0.0f && outputs && outputsCount>0){
                for(int channel=0;channel<std::min(2,outputsCount);++channel){
                    if(auto* out=outputs[channel])
                        for(int frame=0;frame<samples;++frame)
                            out[frame]=juce::jlimit(-0.5f,0.5f,
                                out[frame]+(std::isfinite(source[frame])?source[frame]:0.0f)*monitor);
                }
            }
            const float* mono[] { source };
            if(!slots[i].writer->write(mono,samples))
                rejectedWrites.fetch_add(1,std::memory_order_relaxed);
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
    std::atomic<std::uint64_t> droppedBlocks{0};
    std::atomic<std::uint64_t> rejectedWrites{0};
    std::atomic<float> monitorGain{0.0f};
    std::array<std::atomic<float>,maxTracks> inputPeaks{};
};
