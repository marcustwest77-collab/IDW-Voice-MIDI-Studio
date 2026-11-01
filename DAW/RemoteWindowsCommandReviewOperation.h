#pragma once
#include "RemoteLocalCommandConsent.h"
#include "RemoteWindowsCommandReviewChannel.h"

namespace idw::daw::remote {
// One trusted controller/UI thread owns all methods. Worker never touches JUCE.
// No process launch, numeric-PID trust, TLS verification or default activation.
class WindowsCommandReviewOperation {
public:
    using Deadline=WindowsCommandReviewChannel::Deadline;
    WindowsCommandReviewOperation();
    ~WindowsCommandReviewOperation();
    WindowsCommandReviewOperation(const WindowsCommandReviewOperation&)=delete;
    WindowsCommandReviewOperation& operator=(const WindowsCommandReviewOperation&)=delete;
    // Duplicate the independently trusted borrowed peer synchronously, before
    // scheduling. The caller may release its source handle after start returns.
    // True means scheduled, not allowed/delivered/executed. No overlapping start.
    bool start(void* heldPeer,const LocalTLSOffer&,const std::string& peerId,
               unsigned timeoutMs=2000,Deadline ownerDeadline=Deadline::max());
    bool busy() const; // Remains owned after completion until stop joins/releases.
    std::wstring endpoint() const; // Cached metadata only while the attempt is live.
    std::optional<LocalCommandPrompt> pending() const;
    bool resolve(const LocalCommandPrompt&,bool allow);
    std::optional<bool> takeResult(); // One exchange-completion diagnostic, not client acceptance/authority.
    void stop(); // Close mailbox, cancel/drain/join worker, release owned handles.
    // Owner must stop on session/host revocation. Stop never terminates the peer.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
