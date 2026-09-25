#pragma once
#include "RollingMidiHistory.h"
#include "MidiExport.h"
// Audio writes a bounded SPSC queue; only the timer / UI allocate or export files.
class RetrospectiveCapture final:private juce::Timer {
public:
    RetrospectiveCapture(){startTimer(100);}
    ~RetrospectiveCapture() override{stopTimer();}
    void resetAudio(){resetPending=true;} // called while audio is stopped, before next processBlock
    void finishBlock(const juce::MidiBuffer& midi,int samples,double sr,bool enabled){
        if(sr<=0||samples<0)return;
        if(resetPending||enabled!=wasEnabled){generation.fetch_add(1);resetPending=false;wasEnabled=enabled;if(!enabled)dropped.store(false);}
        enabledPublished.store(enabled);
        if(enabled)for(const auto metadata:midi){
            const auto message=metadata.getMessage();if(message.getRawDataSize()>3)continue;
            int a,b,c,d;fifo.prepareToWrite(1,a,b,c,d);
            if(!b){generation.fetch_add(1);dropped.store(true);continue;}
            auto& item=queue[(size_t)a];item.generation=generation.load();item.event.seconds=audioTime+metadata.samplePosition/sr;
            item.event.size=message.getRawDataSize();std::copy_n(message.getRawData(),item.event.size,item.event.data.begin());fifo.finishedWrite(1);
        }
        audioTime+=samples/sr;clock.store(audioTime);enabledPublished.store(enabled);
    }
    void drain(){const juce::ScopedLock guard(lock);
        auto current=generation.load();if(current!=consumerGeneration){history.clear(clock.load());consumerGeneration=current;fresh=true;}
        for(;;){int a,b,c,d;fifo.prepareToRead(1,a,b,c,d);if(!b)break;const auto item=queue[(size_t)a];fifo.finishedRead(1);
            current=generation.load();if(current!=consumerGeneration){history.clear(item.event.seconds);consumerGeneration=current;fresh=true;}
            if(item.generation==current){
                // The first retained event after a reset establishes the processing-time origin.
                if(fresh){history.clear(item.event.seconds);fresh=false;}
                history.append(item.event);
            }else fresh=true;
        }
        if(!enabledPublished.load()){history.clear(clock.load());fresh=true;}
        history.advance(clock.load());
    }
    std::vector<PerformanceCapture::Event> snapshot(){const juce::ScopedLock guard(lock);drain();std::vector<PerformanceCapture::Event> result;
        for(const auto& e:history.snapshot()){PerformanceCapture::Event converted;converted.seconds=e.seconds;converted.size=e.size;std::copy_n(e.data.begin(),e.size,converted.data);result.push_back(converted);}return result;
    }
    double duration(){const juce::ScopedLock guard(lock);drain();return history.duration();}
    bool incomplete() const{const juce::ScopedLock guard(lock);return dropped.load()||history.capacityLimited();}
private:
    void timerCallback() override{drain();}
    struct Item{uint64_t generation=0;idw::HistoryEvent event;};
    std::array<Item,32768>queue{};juce::AbstractFifo fifo{32768};
    std::atomic<uint64_t>generation{0};std::atomic<double>clock{0};std::atomic<bool>enabledPublished{false},dropped{false};
    uint64_t consumerGeneration=0;double audioTime=0;bool resetPending=true,wasEnabled=false,fresh=true;
    mutable juce::CriticalSection lock;idw::RollingMidiHistory history;
};
