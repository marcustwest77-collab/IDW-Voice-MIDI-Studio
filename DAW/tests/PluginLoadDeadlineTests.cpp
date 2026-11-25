#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
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
// Pumps the message loop until `ready` holds, then reports whether it did so
// before `timeoutMs`. Waiting on the expected state rather than on a fixed
// timestamp keeps a slow machine from turning a pass into a flake.
class Waiter : public std::enable_shared_from_this<Waiter> {
public:
    Waiter(std::function<bool()> ready,int timeoutMs,std::function<void(bool)> done)
        : ready(std::move(ready)),done(std::move(done)),
          deadline(juce::Time::getMillisecondCounter()+(juce::uint32)timeoutMs){}
    void start(){self=shared_from_this();tick();}
private:
    void tick(){
        if(ready()){finish(true);return;}
        if(juce::Time::getMillisecondCounter()>=deadline){finish(false);return;}
        juce::Timer::callAfterDelay(10,[weak=weak_from_this()]{if(auto waiter=weak.lock())waiter->tick();});
    }
    void finish(bool within){
        auto alive=std::move(self);self=nullptr;
        done(within);
    }
    std::function<bool()> ready;
    std::function<void(bool)> done;
    juce::uint32 deadline;
    std::shared_ptr<Waiter> self;
};
void await(std::function<bool()> ready,int timeoutMs,std::function<void(bool)> done){
    std::make_shared<Waiter>(std::move(ready),timeoutMs,std::move(done))->start();
}
// A 20 ms deadline only has to be given room to misfire; this is that room.
constexpr int strayDeadlineRoomMs=1500;
}
int main(){
    // A real message loop is required: the deadline is a juce::Timer, which is
    // the part of the bound the gate's own unit test cannot exercise.
    juce::ScopedJuceInitialiser_GUI initialiser;
    auto* messages=juce::MessageManager::getInstance();
    Report report;
    PluginLoadGate canceledGate,earlyGate,closedGate,deadlineGate;
    std::unique_ptr<PluginLoadGate::Timeout> canceledVerdict,earlyVerdict,deadlineVerdict,secondVerdict;
    bool earlyLoaded=false,lateDestroyed=false,lateLoaded=false;
    bool closedAccepted=false,closedDestroyed=false,closedTimedOut=false;
    juce::uint32 deadlineStarted=0;
    int deadlineToken=0;
    // Held at this scope: these callbacks outlive the timer that arms them.
    std::function<void(std::unique_ptr<Probe>,const juce::String&)> lateCallback;

    std::function<void()> phaseEarlyLoad,phaseProduction,phaseSecondDeadline,finishUp;

    // 1. A cancel request seen by the deadline reports canceled, not failed.
    const int canceledToken=canceledGate.begin();
    armPluginLoadDeadline(canceledGate,canceledToken,80,[]{return true;},
        [&](PluginLoadGate::Timeout verdict){canceledVerdict=std::make_unique<PluginLoadGate::Timeout>(verdict);},
        []{return true;});

    // 2. A load that answers in time must beat its deadline, and a window that
    // closes mid-load must let both the deadline and the late callback go.
    phaseEarlyLoad=[&]{
        const int earlyToken=earlyGate.begin();
        auto earlyCallback=wrapPluginLoadCallback<Probe>(earlyGate,earlyToken,[]{return true;},
            [&](std::unique_ptr<Probe> instance,const juce::String&){earlyLoaded=instance!=nullptr;});
        earlyCallback(makeProbe(nullptr),juce::String());
        // The accepted path hands ownership on, so only delivery is observable.
        report.require(earlyLoaded,"in-time load was not delivered");
        armPluginLoadDeadline(earlyGate,earlyToken,20,[]{return true;},
            [&](PluginLoadGate::Timeout verdict){earlyVerdict=std::make_unique<PluginLoadGate::Timeout>(verdict);},
            []{return false;});

        const int closedToken=closedGate.begin();
        auto closedCallback=wrapPluginLoadCallback<Probe>(closedGate,closedToken,[]{return false;},
            [&](std::unique_ptr<Probe>,const juce::String&){closedAccepted=true;});
        closedCallback(makeProbe(&closedDestroyed),juce::String("closed"));
        armPluginLoadDeadline(closedGate,closedToken,20,[]{return false;},
            [&](PluginLoadGate::Timeout){closedTimedOut=true;},
            []{return false;});

        // Wait for any stray resolution; reaching the timeout is the pass.
        await([&]{return earlyVerdict!=nullptr || closedAccepted || closedTimedOut;},strayDeadlineRoomMs,
            [&](bool resolved){
                report.require(!resolved,"a 20 ms deadline resolved a load it should not have");
                report.require(earlyVerdict==nullptr,"deadline resolved a load that already completed");
                report.require(!closedAccepted,"closed window delivered a late instance");
                report.require(!closedTimedOut,"closed window reported a deadline");
                report.require(closedDestroyed,"closed window kept a late instance alive");
                report.require(closedGate.isWaiting(),"closed window resolved its load");
                phaseProduction();
            });
    };

    // 3. The production bound must actually fire. This waits the real
    // pluginInstantiationTimeoutMs rather than a scaled-down copy.
    phaseProduction=[&]{
        deadlineToken=deadlineGate.begin();
        lateCallback=wrapPluginLoadCallback<Probe>(deadlineGate,deadlineToken,[]{return true;},
            [&](std::unique_ptr<Probe>,const juce::String&){lateLoaded=true;});
        deadlineStarted=juce::Time::getMillisecondCounter();
        armPluginLoadDeadline(deadlineGate,deadlineToken,pluginInstantiationTimeoutMs,[]{return true;},
            [&](PluginLoadGate::Timeout verdict){deadlineVerdict=std::make_unique<PluginLoadGate::Timeout>(verdict);},
            []{return false;});
        await([&]{return deadlineVerdict!=nullptr;},pluginInstantiationTimeoutMs+5000,
            [&](bool arrived){
                report.require(arrived,"the production deadline never fired");
                if(!arrived){finishUp();return;}
                const auto elapsed=juce::Time::getMillisecondCounter()-deadlineStarted;
                report.require(elapsed>=(juce::uint32)pluginInstantiationTimeoutMs-100,
                    "deadline fired before the production bound");
                report.require(*deadlineVerdict==PluginLoadGate::Timeout::failed,
                    "silent load was not reported as failed");
                // The load the deadline abandoned answers afterwards.
                lateCallback(makeProbe(&lateDestroyed),juce::String("too late"));
                report.require(lateDestroyed,"late instance was not torn down");
                report.require(!lateLoaded,"late instance was rendered");
                phaseSecondDeadline();
            });
    };

    // 4. Nothing may resolve the same load a second time.
    phaseSecondDeadline=[&]{
        // Same token: an expired load must not resolve again.
        armPluginLoadDeadline(deadlineGate,deadlineToken,20,[]{return true;},
            [&](PluginLoadGate::Timeout verdict){secondVerdict=std::make_unique<PluginLoadGate::Timeout>(verdict);},
            []{return false;});
        await([&]{return secondVerdict!=nullptr;},strayDeadlineRoomMs,
            [&](bool resolved){
                report.require(!resolved,"deadline resolved the same load twice");
                report.require(deadlineVerdict!=nullptr && *deadlineVerdict==PluginLoadGate::Timeout::failed,
                    "deadline did not stay resolved as failed");
                report.require(lateDestroyed && !lateLoaded,"late callback changed the outcome");
                finishUp();
            });
    };

    finishUp=[&]{messages->stopDispatchLoop();};

    await([&]{return canceledVerdict!=nullptr;},strayDeadlineRoomMs,
        [&](bool arrived){
            report.require(arrived,"cancel verdict never arrived on the message loop");
            report.require(!arrived || *canceledVerdict==PluginLoadGate::Timeout::canceled,
                "cancel request was reported as something other than canceled");
            phaseEarlyLoad();
        });

    messages->runDispatchLoop();
    report.finish("plugin load deadline fires through a message loop and resolves once");
}
