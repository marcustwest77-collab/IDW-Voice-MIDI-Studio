#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace idw {
// Single audio producer, single recognition consumer. Never reset while in use.
class DictationAudioQueue {
public:
    static constexpr unsigned capacity=262144;
    bool push(const float* left,const float* right,int count,int mode) noexcept {
        const auto w=written.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
        if(count<0||static_cast<unsigned>(count)>capacity-(w-r))return false;
        for(int i=0;i<count;++i){
            const float l=left?left[i]:0,rSample=right?right[i]:l;
            float value=mode==1?rSample:mode==2?(l+rSample)*.5f:l;
            if(!std::isfinite(value))value=0;
            samples[(w+static_cast<unsigned>(i))%capacity]=static_cast<short>(std::clamp(value,-1.f,1.f)*32767.f);
        }
        written.store(w+static_cast<unsigned>(count),std::memory_order_release);return true;
    }
    int pop(short* out,int maximum) noexcept {
        const auto r=read.load(std::memory_order_relaxed),w=written.load(std::memory_order_acquire);
        const auto count=std::min(w-r,static_cast<unsigned>(maximum));
        for(unsigned i=0;i<count;++i)out[i]=samples[(r+i)%capacity];
        read.store(r+count,std::memory_order_release);return static_cast<int>(count);
    }
private:
    static_assert(std::atomic<unsigned>::is_always_lock_free,"Audio queue requires lock-free indices");
    std::array<short,capacity> samples{};
    std::atomic<unsigned> written{0},read{0};
};
}
