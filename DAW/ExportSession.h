#pragma once
#include <atomic>
#include <string>
#include "ExportOutcome.h"
#include "PluginLoadGate.h"
namespace idw::daw {
// The export UI as the state machine sees it: running/button state, the CANCEL
// EXPORT caption and the completion alert. Tests implement this instead of
// building a component, which is what makes cancellation headless.
class ExportView {
public:
    virtual ~ExportView()=default;
    virtual void exportRunningChanged(bool running)=0;
    virtual void exportCancelText(const std::string& text)=0;
    virtual void exportAlert(ExportMediaKind media,ExportOutcome outcome,const std::string& message)=0;
};
// One export at a time. It owns the running/cancel state and the plugin-load
// gate, and every path - renderer, cancel button, plugin deadline - resolves
// through one place, so an export can only report once.
class ExportSession {
public:
    enum class Start { started, alreadyRunning };
    enum class Cancel { notRunning, requested, resolvedAsCanceled };
    ExportSession(ExportView& view,int pluginLoadTimeoutSeconds)
        : view(view),timeoutSeconds(pluginLoadTimeoutSeconds){}
    Start start(ExportMediaKind kind){
        if(runningState.load())return Start::alreadyRunning;
        mediaKind=kind;runningState.store(true);cancelRequestedState=false;
        view.exportRunningChanged(true);
        view.exportCancelText("CANCEL EXPORT");
        return Start::started;
    }
    // Safe to read from any thread; the window polls it from paint and timers.
    bool running() const {return runningState.load();}
    bool cancelRequested() const {return cancelRequestedState;}
    ExportMediaKind media() const {return mediaKind;}
    // A pending plugin load cannot be interrupted, so this abandons it and
    // resolves now; a running render is left to observe the flag and finish.
    Cancel cancel(){
        if(!runningState.load())return Cancel::notRunning;
        cancelRequestedState=true;
        view.exportCancelText("CANCELING...");
        if(!gate.cancel())return Cancel::requested;
        resolve(ExportOutcome::canceled,"Export canceled.");
        return Cancel::resolvedAsCanceled;
    }
    // The deadline's verdict: a cancel requested first still wins.
    bool resolvePluginTimeout(){
        if(cancelRequestedState)return finish(ExportOutcome::canceled,"Export canceled.");
        return finish(ExportOutcome::failed,
            "The track effect did not finish loading within "+std::to_string(timeoutSeconds)+
            " seconds, so the export was not written.");
    }
    // Resolves a run exactly once. A late render callback or a second deadline
    // cannot alert over the outcome that already happened.
    bool finish(ExportOutcome outcome,std::string message){
        if(!runningState.load())return false;
        resolve(outcome,std::move(message));
        return true;
    }
    PluginLoadGate& pluginLoad(){return gate;}
private:
    void resolve(ExportOutcome outcome,const std::string& message){
        runningState.store(false);cancelRequestedState=false;
        view.exportRunningChanged(false);
        view.exportCancelText("CANCEL EXPORT");
        view.exportAlert(mediaKind,outcome,message);
    }
    ExportView& view;
    PluginLoadGate gate;
    int timeoutSeconds;
    ExportMediaKind mediaKind=ExportMediaKind::wav;
    std::atomic<bool> runningState{false};
    bool cancelRequestedState=false;
};
}
