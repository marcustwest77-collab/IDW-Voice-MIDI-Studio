#pragma once
#include <functional>
#include <memory>
#include <utility>
#include <juce_events/juce_events.h>
#include "PluginLoadGate.h"
namespace idw::daw {
// How long a track effect gets to finish loading before the WAV export is
// reported as failed instead of waiting on it forever.
inline constexpr int pluginInstantiationTimeoutMs=15000;
// Schedules one load's deadline on the message thread. `stillAlive` is checked
// before the gate is touched, so a closed window can drop its timer; the gate
// then decides whether this token is still the load being waited on. onTimeout
// runs only for failed or canceled, never for a load that already completed.
inline void armPluginLoadDeadline(PluginLoadGate& gate,int token,int timeoutMs,
    std::function<bool()> stillAlive,
    std::function<void(PluginLoadGate::Timeout)> onTimeout,
    std::function<bool()> cancelRequested){
    juce::Timer::callAfterDelay(timeoutMs,
        [&gate,token,alive=std::move(stillAlive),timedOut=std::move(onTimeout),
         canceled=std::move(cancelRequested)]() mutable {
            if(!alive())return;
            const auto verdict=gate.expire(token,canceled());
            if(verdict!=PluginLoadGate::Timeout::ignored)timedOut(verdict);
        });
}
// Adapts a creator's callback to the gate. An instance delivered for a late or
// superseded load is destroyed instead of being rendered, and a closed window
// drops it without touching the gate.
template <typename Instance>
std::function<void(std::unique_ptr<Instance>,const juce::String&)>
wrapPluginLoadCallback(PluginLoadGate& gate,int token,std::function<bool()> stillAlive,
    std::function<void(std::unique_ptr<Instance>,const juce::String&)> accepted){
    return [&gate,token,alive=std::move(stillAlive),accepted=std::move(accepted)]
        (std::unique_ptr<Instance> instance,const juce::String& failure) mutable {
            if(!alive() || !gate.complete(token).accepted)return; // destroyed here
            accepted(std::move(instance),failure);
        };
}
}
