#pragma once
// Process-verified private reviewer -> trusted local UI. No launch/activation.
#include "RemoteCommandReviewWire.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

namespace idw::daw::remote {
struct LocalCommandPrompt {
    // In-process lifetime token only; never serialize or accept it from a peer.
    // Holding a snapshot prevents address reuse across replacement mailboxes.
    std::shared_ptr<const int> owner;
    LocalCommandProposal proposal;
};
class LocalCommandConsent {
public:
    using Clock=std::chrono::steady_clock;
    LocalCommandConsent()=default;
    LocalCommandConsent(const LocalCommandConsent&)=delete;
    LocalCommandConsent& operator=(const LocalCommandConsent&)=delete;
    // One request per instance, off UI/audio threads. Caller cancels/closes and
    // joins the worker before disposing mailbox/cancel state. True means a live
    // explicit decision; allow=false is denial, not a transport/worker failure.
    bool request(const LocalCommandProposal& proposal,bool& allow,const std::atomic<bool>& canceled,
                 Clock::time_point deadline) {
        CommandReviewBytes encoded;const auto now=Clock::now();
        if(!encodeCommandProposal(proposal,encoded) || deadline<=now || deadline-now>std::chrono::seconds(5)) return false;
        std::unique_lock<std::mutex> lock(mutex_);
        if(used_ || closed_ || canceled.load()) return false;
        proposal_=proposal;encoded_=std::move(encoded);used_=true;waiting_=true;canceled_=&canceled;deadline_=deadline;
        struct Retire {LocalCommandConsent& self;~Retire(){self.waiting_=false;self.canceled_=nullptr;}} retire{*this};
        while(active() && !decided_) changed_.wait_until(lock,std::min(deadline_,Clock::now()+std::chrono::milliseconds(10)));
        const bool decided=active() && decided_;
        if(decided) allow=allow_;
        return decided;
    }
    std::optional<LocalCommandPrompt> pending() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!active() || decided_) return std::nullopt;
        return LocalCommandPrompt{owner_,proposal_};
    }
    bool resolve(const LocalCommandPrompt& reviewed,bool allow) {
        CommandReviewBytes encoded;
        if(reviewed.owner!=owner_ || !encodeCommandProposal(reviewed.proposal,encoded)) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(!active() || decided_ || encoded!=encoded_) return false;
        allow_=allow;decided_=true;changed_.notify_all();return true;
    }
    void close(){std::lock_guard<std::mutex> lock(mutex_);closed_=true;changed_.notify_all();}
private:
    bool active() const {return waiting_ && !closed_ && canceled_ && !canceled_->load() && Clock::now()<deadline_;}
    const std::shared_ptr<const int> owner_=std::make_shared<const int>(0);
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    LocalCommandProposal proposal_;
    CommandReviewBytes encoded_;
    const std::atomic<bool>* canceled_=nullptr;
    Clock::time_point deadline_{};
    bool used_=false,waiting_=false,closed_=false,decided_=false,allow_=false;
};
}
