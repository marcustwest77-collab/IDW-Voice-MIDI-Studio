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
 // Sustain all 128 notes through warm-up and every measured buffer size.
 // At 120 BPM, 16 beats = 8 seconds; the longest run is ~3.07 seconds.
 Session session;
 for(int track=0;track<16;++track){
    session.addTrack(TrackType::midi);
    session.setTrackSynthPreset((std::size_t)track,track%5);
    for(int note=0;note<8;++note)
       session.addMidiNote((std::size_t)track,{48+(track+note)%36,0.0,16.0,100});
 }

// Collect 256 timed blocks for each alternate buffer size after warm-up.
 for(const int smallBlock : {64,128,512}){
    constexpr int n=256,warmup=32;
    std::vector<float> l((std::size_t)smallBlock,0.0f),r((std::size_t)smallBlock,0.0f);
    std::array<double,n> times{};
    const double budget=1000.0*smallBlock/48000.0;
    double total=0.0;
    double finalLeftEnergy=0.0,finalRightEnergy=0.0;
    int missed=0;
    int firstMissedBlock=-1;
    int lastMissedBlock=-1;
    double excessMs=0.0;
    double worstOverrunMs=0.0;
    for(int i=-warmup;i<n;++i){
       std::fill(l.begin(),l.end(),0.0f);
       std::fill(r.begin(),r.end(),0.0f);
       const auto before=std::chrono::steady_clock::now();
       renderMidiSine(session,l.data(),r.data(),smallBlock,
                      (std::int64_t)(i+warmup)*smallBlock,48000.0);
       const double ms=std::chrono::duration<double,std::milli>(
           std::chrono::steady_clock::now()-before).count();
       if(i>=0){
          times[(std::size_t)i]=ms;
          total+=ms;
          if(ms>budget){if(firstMissedBlock<0)firstMissedBlock=i;lastMissedBlock=i;++missed;excessMs+=ms-budget;worstOverrunMs=std::max(worstOverrunMs,ms-budget);}
       }
       for(int j=0;j<smallBlock;++j){
          if(!std::isfinite(l[(std::size_t)j]) ||
             !std::isfinite(r[(std::size_t)j]) ||
             std::abs(l[(std::size_t)j])>1.0f ||
             std::abs(r[(std::size_t)j])>1.0f)return 1;
          if(i==n-1){finalLeftEnergy+=std::abs(l[(std::size_t)j]);finalRightEnergy+=std::abs(r[(std::size_t)j]);}
       }
    }
    // Guard against an accidentally silent workload invalidating timing results.
    if(finalLeftEnergy<=0.0 || finalRightEnergy<=0.0){
       std::cerr<<"Silent final MIDI block at buffer_size="<<smallBlock<<"\n";
       return 2;
    }
    std::sort(times.begin(),times.end());
    std::cout<<"buffer_size="<<smallBlock<<" avg_block_ms="<<(total/n)
             <<" p95_block_ms="<<times[n*95/100-1]
             <<" p99_block_ms="<<times[n*99/100-1]
             <<" worst_block_ms="<<times.back()
             <<" block_budget_ms="<<budget
             <<" over_budget_blocks="<<missed
             <<" total_overrun_ms="<<excessMs
             <<" worst_overrun_ms="<<worstOverrunMs
             <<" first_over_budget_block="<<firstMissedBlock
             <<" last_over_budget_block="<<lastMissedBlock<<"\n";
 }
 constexpr int block=256, iterations=256, warmupBlocks=32;
 std::vector<float> left(block,0.0f),right(block,0.0f);
 double worstBlockMs=0.0;
 double bestBlockMs=std::numeric_limits<double>::infinity();
 double totalRenderMs=0.0;
 constexpr double blockBudgetMs=1000.0*block/48000.0;
 int overBudgetBlocks=0;
 double lastLeftEnergy=0.0,lastRightEnergy=0.0;
 std::array<double,iterations> blockTimes{};
 // Warm up code paths and caches before collecting timed blocks.
 for(int i=0;i<warmupBlocks;++i){
    std::fill(left.begin(),left.end(),0.0f);
    std::fill(right.begin(),right.end(),0.0f);
    renderMidiSine(session,left.data(),right.data(),block,(std::int64_t)i*block,48000.0);
 }
 const auto start=std::chrono::steady_clock::now();
 for(int i=0;i<iterations;++i){
    std::fill(left.begin(),left.end(),0.0f);
    std::fill(right.begin(),right.end(),0.0f);
    const auto blockStart=std::chrono::steady_clock::now();
    renderMidiSine(session,left.data(),right.data(),block,(std::int64_t)(i+warmupBlocks)*block,48000.0);
    const double blockMs=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-blockStart).count();
    worstBlockMs=std::max(worstBlockMs,blockMs);
    bestBlockMs=std::min(bestBlockMs,blockMs);
    totalRenderMs+=blockMs;
    blockTimes[(std::size_t)i]=blockMs;
    if(blockMs>blockBudgetMs)++overBudgetBlocks;
    for(int sample=0;sample<block;++sample){
       if(!std::isfinite(left[sample]) || !std::isfinite(right[sample]) ||
          std::abs(left[sample])>1.0f || std::abs(right[sample])>1.0f)return 1;
       if(i==iterations-1){lastLeftEnergy+=std::abs(left[sample]);lastRightEnergy+=std::abs(right[sample]);}
    }
 }
 if(lastLeftEnergy<=0.0 || lastRightEnergy<=0.0){
    std::cerr<<"Silent final MIDI block at buffer_size=256\n";
    return 2;
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
