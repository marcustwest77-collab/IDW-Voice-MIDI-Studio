#include "MidiSineSynth.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <iostream>
#include <limits>
#include <array>
#include <vector>
int main(){
 using namespace idw::daw;
 Session session;
 for(int track=0;track<16;++track){
    session.addTrack(TrackType::midi);
    session.setTrackSynthPreset((std::size_t)track,track%5);
    for(int note=0;note<8;++note)
       session.addMidiNote((std::size_t)track,{48+(track+note)%36,0.0,4.0,100});
 }
 constexpr int block=256, iterations=256;
 std::vector<float> left(block,0.0f),right(block,0.0f);
 double worstBlockMs=0.0;
 double bestBlockMs=std::numeric_limits<double>::infinity();
 double totalRenderMs=0.0;
 constexpr double blockBudgetMs=1000.0*block/48000.0;
 int overBudgetBlocks=0;
 std::array<double,iterations> blockTimes{};
 const auto start=std::chrono::steady_clock::now();
 for(int i=0;i<iterations;++i){
    std::fill(left.begin(),left.end(),0.0f);
    std::fill(right.begin(),right.end(),0.0f);
    const auto blockStart=std::chrono::steady_clock::now();
    renderMidiSine(session,left.data(),right.data(),block,(std::int64_t)i*block,48000.0);
    const double blockMs=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-blockStart).count();
    worstBlockMs=std::max(worstBlockMs,blockMs);
    bestBlockMs=std::min(bestBlockMs,blockMs);
    totalRenderMs+=blockMs;
    blockTimes[(std::size_t)i]=blockMs;
    if(blockMs>blockBudgetMs)++overBudgetBlocks;
    for(int sample=0;sample<block;++sample)
       if(!std::isfinite(left[sample]) || !std::isfinite(right[sample]))return 1;
 }
 const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 std::sort(blockTimes.begin(),blockTimes.end());
 const double p95BlockMs=blockTimes[(iterations*95/100)-1];
 const double p99BlockMs=blockTimes[(iterations*99/100)-1];
 const double audioMs=1000.0*block*iterations/48000.0;
 std::cout<<"128-note synth benchmark: render_ms="<<elapsed
          <<" audio_ms="<<audioMs<<" ratio="<<elapsed/audioMs
          <<" render_only_ms="<<totalRenderMs
          <<" avg_block_ms="<<(totalRenderMs/iterations)
          <<" best_block_ms="<<bestBlockMs
          <<" p95_block_ms="<<p95BlockMs
          <<" p99_block_ms="<<p99BlockMs
          <<" worst_block_ms="<<worstBlockMs
          <<" block_budget_ms="<<blockBudgetMs
          <<" over_budget_blocks="<<overBudgetBlocks
          <<" over_budget_pct="<<(100.0*overBudgetBlocks/iterations)<<"\n";
 return 0;
}
