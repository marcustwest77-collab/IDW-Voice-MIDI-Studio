#pragma once
#include "RemoteCommandReviewWire.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace idw::daw::remote {
// Explicit, single-transaction private channel. No launch/discovery/activation.
// Caller holds independently trusted peer process handles through the call.
class WindowsCommandReviewChannel {
public:
    using Deadline=std::chrono::steady_clock::time_point;
    // True means an explicit decision was obtained; allow=false is a real denial.
    // Run off UI/audio thread. Callback must honor cancellation/deadline, never
    // retain references or re-enter this channel. No remote peer supplies it.
    using Reviewer=std::function<bool(const LocalCommandProposal&,bool& allow,const std::atomic<bool>&,Deadline)>;
    WindowsCommandReviewChannel();
    ~WindowsCommandReviewChannel();
    WindowsCommandReviewChannel(const WindowsCommandReviewChannel&)=delete;
    WindowsCommandReviewChannel& operator=(const WindowsCommandReviewChannel&)=delete;
    bool create();
    std::wstring name() const;
    bool reviewFrom(void* heldClientProcess,const LocalTLSOffer& expected,const std::string& expectedPeerId,
                    const Reviewer&,unsigned timeoutMs=2000,Deadline ownerDeadline=Deadline::max());
    void cancel();
    void close(); // Cancel, wait for in-flight I/O, then dispose; owner joins its worker.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
