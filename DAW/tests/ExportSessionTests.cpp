#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "ExportSession.h"
using namespace idw::daw;
namespace {
void require(bool condition,const char* message){
    if(!condition)throw std::runtime_error(message);
}
struct Alert {
    ExportMediaKind media;
    ExportOutcome outcome;
    std::string message;
};
struct FakeView : ExportView {
    std::vector<bool> runningChanges;
    std::vector<std::string> cancelTexts;
    std::vector<Alert> alerts;
    void exportRunningChanged(bool running) override {runningChanges.push_back(running);}
    void exportCancelText(const std::string& text) override {cancelTexts.push_back(text);}
    void exportAlert(ExportMediaKind media,ExportOutcome outcome,const std::string& message) override {
        alerts.push_back({media,outcome,message});
    }
    bool showsRunning() const {return !runningChanges.empty() && runningChanges.back();}
};
}
int main(){
    // Starting twice is refused, so a second click cannot clobber a live run.
    {
        FakeView view;ExportSession session(view,15);
        require(session.start(ExportMediaKind::wav)==ExportSession::Start::started,"first start refused");
        require(view.showsRunning(),"view never learned the export started");
        require(session.start(ExportMediaKind::midi)==ExportSession::Start::alreadyRunning,"second start accepted");
        require(view.alerts.empty(),"starting raised an alert");
    }
    // Cancelling during a plugin load resolves immediately and tears the wait down.
    {
        FakeView view;ExportSession session(view,15);
        session.start(ExportMediaKind::wav);
        session.pluginLoad().begin();
        require(session.cancel()==ExportSession::Cancel::resolvedAsCanceled,
            "cancel during a plugin load did not resolve");
        require(!session.running(),"canceled export still running");
        require(view.alerts.size()==1 && view.alerts[0].outcome==ExportOutcome::canceled,
            "cancel alert did not report canceled");
        require(view.alerts[0].media==ExportMediaKind::wav,"cancel alert used the wrong media");
        require(view.cancelTexts.back()=="CANCEL EXPORT","button text not restored after cancel");
    }
    // Cancelling a render only requests it; the renderer resolves, once.
    {
        FakeView view;ExportSession session(view,15);
        session.start(ExportMediaKind::wav);
        require(session.cancel()==ExportSession::Cancel::requested,"render cancel resolved early");
        require(session.running() && session.cancelRequested(),"cancel request not recorded");
        require(view.cancelTexts.back()=="CANCELING...","button did not show the cancel request");
        require(session.finish(ExportOutcome::canceled,"Export canceled."),"canceled finish refused");
        require(!session.finish(ExportOutcome::failed,"late"),"a second finish was accepted");
        require(view.alerts.size()==1,"late finish produced a second alert");
        require(view.alerts[0].outcome==ExportOutcome::canceled,"wrong outcome alerted");
    }
    // A cancel that arrives too late must not turn a completed export into a
    // failure: the renderer's outcome is the truth.
    {
        FakeView view;ExportSession session(view,15);
        session.start(ExportMediaKind::wav);
        require(session.cancel()==ExportSession::Cancel::requested,"render cancel resolved early");
        require(session.finish(ExportOutcome::completed,"WAV written."),
            "completed finish was refused after a cancel request");
        require(view.alerts.size()==1,"late cancel raised more than one alert");
        require(view.alerts[0].outcome==ExportOutcome::completed,
            "late cancel turned a completed export into a failure");
        require(view.alerts[0].message=="WAV written.","completed message was replaced");
        require(!session.running(),"completed export still running");
        require(!session.cancelRequested(),"cancel request survived completion");
    }
    // A silent plugin is reported as a failed export after the configured bound.
    {
        FakeView view;ExportSession session(view,15);
        session.start(ExportMediaKind::wav);
        session.pluginLoad().begin();
        require(session.resolvePluginTimeout(),"deadline did not resolve the export");
        require(view.alerts.size()==1 && view.alerts[0].outcome==ExportOutcome::failed,
            "deadline did not report a failure");
        require(view.alerts[0].message.find("15 seconds")!=std::string::npos,
            "deadline alert omitted the bound");
        require(!session.resolvePluginTimeout(),"deadline resolved the export twice");
        require(view.alerts.size()==1,"second deadline raised another alert");
    }
    // A cancel that arrives before the deadline wins the same race.
    {
        FakeView view;ExportSession session(view,15);
        session.start(ExportMediaKind::wav);
        session.pluginLoad().begin();
        require(session.cancel()==ExportSession::Cancel::resolvedAsCanceled,"cancel did not resolve the load");
        require(!session.resolvePluginTimeout(),"deadline overrode the cancel");
        require(view.alerts.size()==1 && view.alerts[0].outcome==ExportOutcome::canceled,
            "deadline replaced the canceled alert");
    }
    // A finished export can be started again, and the new run is independent.
    {
        FakeView view;ExportSession session(view,15);
        session.start(ExportMediaKind::wav);
        session.finish(ExportOutcome::completed,"WAV written.");
        require(!session.running(),"completed export still running");
        require(session.start(ExportMediaKind::midi)==ExportSession::Start::started,"re-export refused");
        require(session.media()==ExportMediaKind::midi,"re-export kept the old media");
        require(!session.cancelRequested(),"re-export inherited the old cancel request");
        require(session.cancel()==ExportSession::Cancel::requested,"re-export had no live load to cancel");
        require(view.alerts.size()==1,"re-export raised an alert before finishing");
    }
    // Cancelling with nothing running is a no-op.
    {
        FakeView view;ExportSession session(view,15);
        require(session.cancel()==ExportSession::Cancel::notRunning,"idle cancel was accepted");
        require(session.finish(ExportOutcome::canceled,"x")==false,"idle finish was accepted");
        require(view.alerts.empty(),"idle session raised an alert");
    }
    std::cout<<"PASS: export cancel, timeout and re-export resolve through one state machine\n";
}
