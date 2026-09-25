#pragma once
#include <JuceHeader.h>
#include <array>
// Audio produces short MIDI messages; the editor drains them on its timer.
// Disk IO and MIDI-file construction never run on the audio thread.
class PerformanceCapture {
public:
    struct Event { double seconds=0; unsigned char data[3]{}; int size=0; };
    void start(){ elapsed.store(0);overflow.store(false);recording.store(true);requested.store(true); }
    void stop(){ requested.store(false); }
    double duration() const { return elapsed.load(); }
    bool isRecording() const { return recording.load(); }
    bool isActive() const { return active; } // audio-thread only
    bool wasTruncated() const { return overflow.load(); }
    void beginBlock(double sr){
        if(requested.load() && !active){active=true;seconds=0;}
        sampleRate=sr;
    }
    void finishBlock(const juce::MidiBuffer& midi,int samples){
        if(active){for(const auto metadata:midi){const auto msg=metadata.getMessage();if(msg.getRawDataSize()<=3)push(msg,seconds+metadata.samplePosition/sampleRate);}
            seconds+=samples/sampleRate;elapsed.store(seconds);
            if(!requested.load() || seconds>=600.0 || overflow.load()){
                // Close notes in the file without interrupting the live instrument.
                for(int channel=1;channel<=16;++channel)push(juce::MidiMessage::allNotesOff(channel),seconds);
                active=false;requested.store(false);recording.store(false);
            }
        } else if(!requested.load())recording.store(false);
    }
    bool pop(Event& event){int a,b,c,d;fifo.prepareToRead(1,a,b,c,d);if(!b)return false;event=events[(size_t)a];fifo.finishedRead(1);return true;}
    void resetAudio(){active=false;requested.store(false);recording.store(false);} // device restart stops capture
private:
    void push(const juce::MidiMessage& msg,double time){int a,b,c,d;fifo.prepareToWrite(1,a,b,c,d);if(!b){overflow.store(true);return;}
        auto& e=events[(size_t)a];e.seconds=time;e.size=msg.getRawDataSize();std::copy_n(msg.getRawData(),e.size,e.data);fifo.finishedWrite(1);}
    juce::AbstractFifo fifo{8192};std::array<Event,8192> events{};
    std::atomic<double> elapsed{0};
    std::atomic<bool> requested{false},recording{false},overflow{false};
    bool active=false;double seconds=0,sampleRate=48000;
};
