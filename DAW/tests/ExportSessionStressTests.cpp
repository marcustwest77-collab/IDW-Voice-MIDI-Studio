// Multithreaded stress test for ExportSession.
//
// The session is written on the message thread and read from everywhere: paint,
// timers, the remote state builders and the renderers that poll cancelFlag().
// This test exercises exactly that contract (one writer, many readers) and is
// meant to be run under ThreadSanitizer:
//
//   g++ -std=c++17 -O1 -g -fsanitize=thread -Wall -Wextra -Werror -I DAW
//       DAW/tests/ExportSessionStressTests.cpp -o /tmp/session_stress -pthread
//
// It does not call two mutators concurrently: that is not the contract, and a
// race there would be a test bug rather than a session bug.
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include "ExportSession.h"
using namespace idw::daw;
namespace {
void require(bool condition,const std::string& message){
    if(!condition)throw std::runtime_error(message);
}
// Written only by the session's writer thread; readers never touch it.
class RecordingView : public ExportView {
public:
    std::vector<ExportOutcome> alerts;
    void exportRunningChanged(bool) override {}
    void exportCancelText(const std::string&) override {}
    void exportAlert(ExportMediaKind,ExportOutcome outcome,const std::string&) override {alerts.push_back(outcome);}
};
// Spins the way a renderer does: polls the flag while it runs, stops on cancel
// or when told to finish. Never mutates the session.
std::thread startRenderer(const ExportSession& session,std::atomic<bool>& rendering,std::atomic<bool>& stop){
    return std::thread([&session,&rendering,&stop]{
        rendering.store(true);
        while(!session.cancelRequested() && !stop.load())std::this_thread::yield();
    });
}
void waitForRenderer(const std::atomic<bool>& rendering){
    for(int i=0;i<100000 && !rendering.load();++i)std::this_thread::yield();
    require(rendering.load(),"renderer never started");
}
}
int main(){
    constexpr int churnRuns=200;
    constexpr int handoffRuns=100;
    RecordingView view;
    ExportSession session(view,15);
    // Readers: the paint/timer/remote paths, reading only atomics.
    std::atomic<bool> stopReaders{false};
    std::atomic<unsigned long> reads{0};
    std::vector<std::thread> readers;
    for(int i=0;i<4;++i){
        readers.emplace_back([&session,&stopReaders,&reads]{
            while(!stopReaders.load(std::memory_order_relaxed)){
                (void)session.running();
                (void)session.cancelRequested();
                (void)session.cancelFlag().load();
                reads.fetch_add(1,std::memory_order_relaxed);
            }
        });
    }
    int runs=0;
    // Churn: start, run a renderer, then either cancel it or let it finish.
    for(int i=0;i<churnRuns;++i){
        const auto kind=(i%2)?ExportMediaKind::midi:ExportMediaKind::wav;
        require(session.start(kind)==ExportSession::Start::started,"start refused a clean session");
        ++runs;
        std::atomic<bool> rendering{false},stop{false};
        auto renderer=startRenderer(session,rendering,stop);
        waitForRenderer(rendering);
        const bool canceled=(i%3)==0;
        if(canceled){
            require(session.cancel()==ExportSession::Cancel::requested,
                "cancel of a running export did not request");
            renderer.join();
            session.finish(ExportOutcome::canceled,"Export canceled.");
        }else{
            stop.store(true);
            renderer.join();
            session.finish(ExportOutcome::completed,"Written.");
        }
        require(!session.running(),"session still running after resolve");
        require(!session.cancelRequested(),"cancel request survived resolve");
    }
    require(static_cast<int>(view.alerts.size())==runs,"expected one alert per run");
    // Handoff: a cancel request must reach a renderer that is already polling.
    for(int i=0;i<handoffRuns;++i){
        require(session.start(ExportMediaKind::wav)==ExportSession::Start::started,"handoff start refused");
        std::atomic<bool> rendering{false},stop{false};
        auto renderer=startRenderer(session,rendering,stop);
        waitForRenderer(rendering);
        require(session.cancel()==ExportSession::Cancel::requested,"handoff cancel did not request");
        renderer.join();
        session.finish(ExportOutcome::canceled,"Export canceled.");
        require(!session.running() && !session.cancelRequested(),"handoff session did not settle");
        ++runs;
    }
    // Late cancel: the renderer's outcome wins, and only one alert is raised.
    for(int i=0;i<handoffRuns;++i){
        require(session.start(ExportMediaKind::midi)==ExportSession::Start::started,"late-cancel start refused");
        std::atomic<bool> rendering{false},stop{false};
        auto renderer=startRenderer(session,rendering,stop);
        waitForRenderer(rendering);
        require(session.cancel()==ExportSession::Cancel::requested,"late cancel did not request");
        stop.store(true);
        renderer.join();
        session.finish(ExportOutcome::completed,"Written.");
        require(!session.running(),"late-cancel session still running");
        ++runs;
    }
    stopReaders.store(true);
    for(auto& reader:readers)reader.join();
    require(static_cast<int>(view.alerts.size())==runs,"an extra alert was raised");
    require(view.alerts.back()==ExportOutcome::completed,"last alert should be completed");
    require(reads.load()>0,"readers never ran");
    std::cout<<"PASS: "<<runs<<" exports across 4 reader threads, "<<reads.load()<<" reads, one alert per run\n";
}
