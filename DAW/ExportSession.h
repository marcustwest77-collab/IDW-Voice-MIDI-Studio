#pragma once
#include <atomic>
#include <string>
#include <utility>
#include "ExportOutcome.h"
#include "PluginLoadGate.h"
namespace idw::daw {
// The captions the session drives; kept here so the window and the tests agree.
inline constexpr const char* exportCancelCaption="CANCEL EXPORT";
inline constexpr const char* exportCancelingCaption="CANCELING...";
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
// One export at a time. It owns the running state, the cancel request - which
// the renderers poll through cancelFlag() - the plugin-load gate and the single
// resolution every path goes through, so an export can only report once.
class ExportSession {
public:
    enum class Start { started, alreadyRunning };
    enum class Cancel { notRunning, requested, resolvedAsCanceled };
    ExportSession(ExportView& view,int pluginLoadTimeoutSeconds)
        : view(view),timeoutSeconds(pluginLoadTimeoutSeconds){}
    Start start(ExportMediaKind kind){
        if(runningState.load())return Start::alreadyRunning;
        // resolve() is the only path out of a run and clears the flag, so a
        // start cannot inherit one.
        mediaKind=kind;runningState.store(true);
        view.exportRunningChanged(true);
        view.exportCancelText(exportCancelCaption);
        return Start::started;
    }
    // Safe to read from any thread; the window polls it from paint and timers.
    bool running() const {return runningState.load();}
    bool cancelRequested() const {return cancelFlag_.load();}
    // The renderers' cancel flag, owned here so there is only one copy.
    std::atomic<bool>& cancelFlag(){return cancelFlag_;}
    // A pending plugin load cannot be interrupted, so this abandons it and
    // resolves now; a running render is left to observe the flag and finish.
    Cancel cancel(){
        if(!runningState.load())return Cancel::notRunning;
        cancelFlag_.store(true);
        view.exportCancelText(exportCancelingCaption);
        if(!gate.cancel())return Cancel::requested;
        finish(ExportOutcome::canceled,"Export canceled.");
        return Cancel::resolvedAsCanceled;
    }
    // The deadline's verdict: a cancel requested first still wins.
    void resolvePluginTimeout(){
        if(cancelRequested()){finish(ExportOutcome::canceled,"Export canceled.");return;}
        finish(ExportOutcome::failed,
            "The track effect did not finish loading within "+std::to_string(timeoutSeconds)+
            " seconds, so the export was not written.");
    }
    // Resolves a run exactly once. A late render callback or a second deadline
    // cannot alert over the outcome that already happened.
    void finish(ExportOutcome outcome,std::string message){
        if(!runningState.load())return;
        resolve(outcome,std::move(message));
    }
    PluginLoadGate& pluginLoad(){return gate;}
private:
    void resolve(const ExportOutcome outcome,const std::string& message){
        runningState.store(false);cancelFlag_.store(false);
        view.exportRunningChanged(false);
        view.exportCancelText(exportCancelCaption);
        view.exportAlert(mediaKind,outcome,message);
    }
    ExportView& view;
    PluginLoadGate gate;
    int timeoutSeconds;
    ExportMediaKind mediaKind=ExportMediaKind::wav;
    std::atomic<bool> runningState{false};
    std::atomic<bool> cancelFlag_{false};
};
}
