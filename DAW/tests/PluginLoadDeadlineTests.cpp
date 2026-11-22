#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <juce_events/juce_events.h>
#include "PluginLoadDeadline.h"
using namespace idw::daw;
namespace {
// Instance type standing in for juce::AudioPluginInstance: a real one cannot be
// constructed without a plugin, and the gate only needs something to destroy.
struct Probe {
    bool* destroyed;
    ~Probe(){if(destroyed!=nullptr)*destroyed=true;}
};
struct Report {
    std::string failures;
    void require(bool condition,const char* message){
        if(!condition)failures+="FAIL: "+std::string(message)+"\n";
    }
    void finish(const char* success){
        if(!failures.empty()){std::cerr<<failures;std::exit(1);}
        std::cout<<"PASS: "<<success<<"\n";
    }
};
std::unique_ptr<Probe> makeProbe(bool* destroyed){
    return std::unique_ptr<Probe>(new Probe{destroyed});
}
}
int main(){
    // A real message loop is required: the deadline is a juce::Timer, which is
    // the part of the bound the gate's own unit test cannot exercise.
    juce::ScopedJuceInitialiser_GUI initialiser;
    auto* messages=juce::MessageManager::getInstance();
    Report report;
    PluginLoadGate canceledGate,earlyGate,deadlineGate;
    std::unique_ptr<PluginLoadGate::Timeout> canceledVerdict,earlyVerdict,deadlineVerdict;
    bool earlyLoaded=false,lateDestroyed=false,lateLoaded=false;
    juce::uint32 deadlineStarted=0;
    // Held at this scope: the deadline callback outlives the timer that arms it.
    std::function<void(std::unique_ptr<Probe>,const juce::String&)> lateCallback;

    // 1. A cancel request seen by the deadline reports canceled, not failed.
    const int canceledToken=canceledGate.begin();
    armPluginLoadDeadline(canceledGate,canceledToken,80,[]{return true;},
        [&](PluginLoadGate::Timeout verdict){canceledVerdict=std::make_unique<PluginLoadGate::Timeout>(verdict);},
        []{return true;});

    juce::Timer::callAfterDelay(200,[&]{
        report.require(canceledVerdict!=nullptr,"cancel verdict never arrived on the message loop");
        report.require(canceledVerdict!=nullptr && *canceledVerdict==PluginLoadGate::Timeout::canceled,
            "cancel request was reported as something other than canceled");

        // 2. A load that answers in time must beat its deadline.
        const int earlyToken=earlyGate.begin();
        auto callback=wrapPluginLoadCallback<Probe>(earlyGate,earlyToken,[]{return true;},
            [&](std::unique_ptr<Probe> instance,const juce::String&){earlyLoaded=instance!=nullptr;});
        bool destroyed=false;
        callback(makeProbe(&destroyed),juce::String());
        // The accepted path hands ownership on, so only delivery is observable.
        report.require(earlyLoaded,"in-time load was not delivered");
        armPluginLoadDeadline(earlyGate,earlyToken,20,[]{return true;},
            [&](PluginLoadGate::Timeout verdict){earlyVerdict=std::make_unique<PluginLoadGate::Timeout>(verdict);},
            []{return false;});
    });

    juce::Timer::callAfterDelay(400,[&]{
        report.require(earlyVerdict==nullptr,"deadline resolved a load that already completed");

        // 3. The production bound must actually fire. This waits the real
        // pluginInstantiationTimeoutMs rather than a scaled-down copy.
        const int deadlineToken=deadlineGate.begin();
        lateCallback=wrapPluginLoadCallback<Probe>(deadlineGate,deadlineToken,[]{return true;},
            [&](std::unique_ptr<Probe>,const juce::String&){lateLoaded=true;});
        deadlineStarted=juce::Time::getMillisecondCounter();
        armPluginLoadDeadline(deadlineGate,deadlineToken,pluginInstantiationTimeoutMs,[]{return true;},
            [&](PluginLoadGate::Timeout verdict){
                deadlineVerdict=std::make_unique<PluginLoadGate::Timeout>(verdict);
                const auto elapsed=juce::Time::getMillisecondCounter()-deadlineStarted;
                report.require(elapsed>=(juce::uint32)pluginInstantiationTimeoutMs-100,
                    "deadline fired before the production bound");
                report.require(verdict==PluginLoadGate::Timeout::failed,
                    "silent load was not reported as failed");
                // The load the deadline abandoned answers afterwards.
                lateCallback(makeProbe(&lateDestroyed),juce::String("too late"));
                report.require(lateDestroyed,"late instance was not torn down");
                report.require(!lateLoaded,"late instance was rendered");
                // Nothing may resolve the same load a second time.
                armPluginLoadDeadline(deadlineGate,deadlineToken,20,[]{return true;},
                    [&](PluginLoadGate::Timeout again){
                        deadlineVerdict=std::make_unique<PluginLoadGate::Timeout>(again);
                    },
                    []{return false;});
            },
            []{return false;});
    });

    juce::Timer::callAfterDelay(pluginInstantiationTimeoutMs+900,[&]{
        report.require(deadlineVerdict!=nullptr && *deadlineVerdict==PluginLoadGate::Timeout::failed,
            "deadline did not stay resolved as failed");
        report.require(lateDestroyed && !lateLoaded,"late callback changed the outcome");
        messages->stopDispatchLoop();
    });

    messages->runDispatchLoop();
    report.finish("plugin load deadline fires through a message loop and resolves once");
}
