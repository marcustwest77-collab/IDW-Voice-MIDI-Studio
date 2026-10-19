#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace idw {

struct KeySuggestion {
    int root = 0;
    int mode = 0; // 0 major, 1 minor
    float confidence = 0.0f;
    int observations = 0;
    bool valid = false;
    bool learning = false;
};

// Lock-free pitch-class learner: the audio thread only adds atomic weights;
// the UI thread reads a snapshot and applies a key only after user confirmation.
class AutoKeyDetector {
public:
    void start() {
        for (auto& value : histogram) value.store(0.0f, std::memory_order_relaxed);
        count.store(0, std::memory_order_relaxed);
        active.store(true, std::memory_order_release);
    }
    void stop() { active.store(false, std::memory_order_release); }
    bool isLearning() const { return active.load(std::memory_order_acquire); }
    void observe(int midiNote, float quality) {
        if (!isLearning() || midiNote < 0 || midiNote > 127) return;
        const float weight = std::clamp(quality, 0.0f, 1.0f);
        add(histogram[(std::size_t)(midiNote % 12)],weight);
        count.fetch_add(1, std::memory_order_relaxed);
    }
    KeySuggestion suggestion() const {
        KeySuggestion result; result.learning=isLearning(); result.observations=count.load(std::memory_order_relaxed);
        if (result.observations < minimumObservations) return result;
        std::array<float,12> notes{};float total=0.0f;
        for(std::size_t i=0;i<notes.size();++i){notes[i]=histogram[i].load(std::memory_order_relaxed);total+=notes[i];}
        if(total<=0.0f)return result;
        for(auto& value:notes)value/=total;
        static constexpr std::array<float,12> major{{6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f}};
        static constexpr std::array<float,12> minor{{6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f}};
        float best=-1.0e9f,second=-1.0e9f;
        for(int mode=0;mode<2;++mode)for(int root=0;root<12;++root){
            const auto& profile=mode==0?major:minor;float score=0.0f;
            for(int pc=0;pc<12;++pc)score+=notes[(std::size_t)pc]*profile[(std::size_t)((pc-root+12)%12)];
            if(score>best){second=best;best=score;result.root=root;result.mode=mode;}else if(score>second)second=score;
        }
        result.confidence=std::clamp((best-second)/std::max(0.001f,std::abs(best))*4.0f,0.0f,1.0f);
        result.valid=true;return result;
    }
    static constexpr int majorMask=2741;
    static constexpr int minorMask=1453;
private:
    static void add(std::atomic<float>& target,float amount){
        auto current=target.load(std::memory_order_relaxed);
        while(!target.compare_exchange_weak(current,current+amount,std::memory_order_relaxed,std::memory_order_relaxed)){}
    }
    static constexpr int minimumObservations=24;
    std::array<std::atomic<float>,12> histogram{};
    std::atomic<int> count{0};
    std::atomic<bool> active{false};
};

} // namespace idw
