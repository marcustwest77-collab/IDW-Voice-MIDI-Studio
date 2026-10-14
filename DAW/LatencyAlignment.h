#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace idw::daw {
// Pure timing-analysis helper for a known test pulse recorded in loopback.
// Return lag in whole samples relative to the expected pulse position.
// Positive lag: pulse arrives late. Does not modify recorded audio.
struct PulseAlignment {
    bool valid=false;
    int lagSamples=0;
    float peak=0.0f;
};
inline PulseAlignment locateLoopbackPulse(const float* recorded,std::size_t count,
                                          std::size_t expectedSample,
                                          std::size_t searchRadius,float minPeak=0.05f) {
    PulseAlignment result;
    if(!recorded || count==0 || expectedSample>=count ||
       !std::isfinite(minPeak) || minPeak<=0.0f)return result;
    const auto first=expectedSample>searchRadius?expectedSample-searchRadius:0;
    const auto last=std::min(count-1,expectedSample+std::min(searchRadius,count-1-expectedSample));
    std::size_t best=first;
    for(auto i=first;i<=last;++i){
        const float value=recorded[i];
        if(std::isfinite(value) && std::abs(value)>result.peak){
            result.peak=std::abs(value);
            best=i;
        }
    }
    if(result.peak<minPeak)return result;
    const auto delta=static_cast<double>(best)-static_cast<double>(expectedSample);
    if(delta<std::numeric_limits<int>::min() || delta>std::numeric_limits<int>::max())
        return result;
    result.valid=true;
    result.lagSamples=static_cast<int>(delta);
    return result;
}
// Estimate a stable delay from multiple known pulses. Reject outliers beyond
// tolerance; never silently apply an uncertain offset to a user's take.
struct RepeatedPulseAlignment {
    bool valid=false;
    int lagSamples=0;
    std::size_t accepted=0;
    std::size_t total=0;
    int jitterSamples=0; // maximum absolute deviation among accepted pulse lags
};
inline RepeatedPulseAlignment estimateLoopbackLatency(
    const float* recorded,std::size_t count,
    const std::vector<std::size_t>& expectedPulses,
    std::size_t searchRadius,int toleranceSamples=8) {
    RepeatedPulseAlignment result;
    result.total=expectedPulses.size();
    if(!recorded || expectedPulses.size()<3 || toleranceSamples<0 || count==0 ||
       expectedPulses.size()>64 || searchRadius==0)return result;
    // The first/last search window must fit entirely inside the captured WAV.
    // Otherwise truncated capture could be mistaken for a valid calibration.
    if(expectedPulses.front()<searchRadius ||
       expectedPulses.back()>=count ||
       searchRadius>count-1-expectedPulses.back())return result;
    // Overlapping search windows could identify the same transient for
    // multiple pulses and incorrectly report a confident measurement.
    for(std::size_t i=0;i<expectedPulses.size();++i){
        const auto expected=expectedPulses[i];
        if(expected>=count)return result;
        if(i>0){
            const auto previous=expectedPulses[i-1];
            if(expected<=previous ||
               expected-previous<=searchRadius ||
               expected-previous-searchRadius<=searchRadius)return result;
        }
    }
    std::vector<int> delays;
    for(const auto expected:expectedPulses){
        const auto pulse=locateLoopbackPulse(recorded,count,expected,searchRadius);
        if(pulse.valid)delays.push_back(pulse.lagSamples);
    }
    if(delays.size()<3)return result;
    std::sort(delays.begin(),delays.end());
    const int median=delays[delays.size()/2];
    for(const auto delay:delays)
        if(std::abs(static_cast<double>(delay)-median)<=toleranceSamples){
            ++result.accepted;
            result.jitterSamples=std::max(result.jitterSamples,
                static_cast<int>(std::abs(static_cast<double>(delay)-median)));
        }
    // Compute the 75% threshold without multiplication overflow.
    const std::size_t required=result.total-result.total/4;
    if(result.accepted<3 || result.accepted<required)return result;
    // The global median can fall outside the accepted cluster when unusual
    // outliers are present. Use the median of only the accepted lags.
    std::vector<int> acceptedDelays;
    for(const auto delay:delays)
        if(std::abs(static_cast<double>(delay)-median)<=toleranceSamples)
            acceptedDelays.push_back(delay);
    const int stableMedian=acceptedDelays[acceptedDelays.size()/2];
    result.jitterSamples=0;
    for(const auto delay:acceptedDelays)
        result.jitterSamples=std::max(result.jitterSamples,
            static_cast<int>(std::abs(static_cast<double>(delay)-stableMedian)));
    result.valid=true;
    result.lagSamples=stableMedian;
    return result;
}
// Conservative UI quality label; never authorizes applying a correction.
// A measurement with large pulse-to-pulse variation should be repeated.
enum class CalibrationQuality { invalid, repeat, consistent };
inline CalibrationQuality assessCalibration(const RepeatedPulseAlignment& measurement,
                                             double sampleRate,
                                             double maxJitterMs=0.25) {
    if(!measurement.valid || !std::isfinite(sampleRate) || sampleRate<=0.0 ||
       !std::isfinite(maxJitterMs) || maxJitterMs<0.0 ||
       measurement.accepted>measurement.total)return CalibrationQuality::invalid;
    const double jitterMs=1000.0*measurement.jitterSamples/sampleRate;
    return jitterMs<=maxJitterMs ? CalibrationQuality::consistent
                                 : CalibrationQuality::repeat;
}
inline double alignmentMilliseconds(int lagSamples,double sampleRate){
    return std::isfinite(sampleRate) && sampleRate>0.0
        ?1000.0*static_cast<double>(lagSamples)/sampleRate:0.0;
}
}
