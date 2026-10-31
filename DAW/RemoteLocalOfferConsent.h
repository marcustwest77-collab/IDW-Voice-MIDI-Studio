#pragma once
// Background launcher -> trusted local UI/controller. No network or UI activation.
#include "RemoteLocalSessionOwner.h"
#include "RemoteTLSOfferWire.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <optional>
#include <mutex>

namespace idw::daw::remote {
struct LocalOfferPrompt {
    std::uint64_t generation=0; // Local controller instance, not peer-supplied authority.
    LocalTLSOffer offer;
};
class LocalOfferConsent {
public:
    using Clock=std::chrono::steady_clock;
    LocalOfferConsent()=default;
    LocalOfferConsent(const LocalOfferConsent&)=delete;
    LocalOfferConsent& operator=(const LocalOfferConsent&)=delete;
    // One request per instance. Owner must close/cancel and join request's worker
    // before destroying this object, the cancellation flag or controller state.
    // Never call this blocking method on the UI/audio thread. The manager still
    // enforces its own shared bootstrap deadline after this callback returns.
    bool request(const LocalTLSOffer& offer,LocalHostConsent& out,const std::atomic<bool>& canceled,
                 std::chrono::milliseconds timeout=std::chrono::milliseconds(2000)) {
        if(timeout.count()<20 || timeout.count()>5000) return false;
        return requestUntil(offer,out,canceled,Clock::now()+timeout);
    }
    // Managed UI path: preserve the launcher's absolute deadline rather than
    // starting another timeout after TLS/IPC work has already consumed time.
    bool requestUntil(const LocalTLSOffer& offer,LocalHostConsent& out,const std::atomic<bool>& canceled,
                      Clock::time_point deadline) {
        LocalTLSOfferBytes checked{};const auto now=Clock::now();
        if(!encodeTLSOffer(offer,checked) || deadline<=now || deadline-now>std::chrono::seconds(30)) return false;
        std::unique_lock<std::mutex> lock(mutex_);
        if(used_ || closed_ || canceled.load()) return false;
        offer_=offer;used_=true;waiting_=true;canceled_=&canceled;deadline_=deadline;
        while(active() && !decided_) {
            const auto next=std::min(deadline_,Clock::now()+std::chrono::milliseconds(10));
            changed_.wait_until(lock,next);
        }
        const bool accepted=active() && decided_ && approved_;
        waiting_=false;canceled_=nullptr;
        if(accepted) out={offer_.epoch,offer_.peerSha256,offer_.connectionId,true,true};
        return accepted;
    }
    // Snapshot is informational. Only a trusted in-process local decision may
    // call resolve; do not connect this interface to a remote peer message.
    std::optional<LocalTLSOffer> pending() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!active() || decided_) return std::nullopt;
        return offer_;
    }
    bool resolve(const LocalTLSOffer& reviewed,bool allowTrackControl) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!active() || decided_ || reviewed.epoch!=offer_.epoch ||
           reviewed.peerSha256!=offer_.peerSha256 || reviewed.connectionId!=offer_.connectionId ||
           reviewed.tlsVersion!=offer_.tlsVersion) return false;
        approved_=allowTrackControl;decided_=true;changed_.notify_all();return true;
    }
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_=true;changed_.notify_all();
    }
private:
    bool active() const { // mutex held; cancellation reference exists only during request.
        return waiting_ && !closed_ && canceled_ && !canceled_->load() && Clock::now()<deadline_;
    }
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    LocalTLSOffer offer_;
    const std::atomic<bool>* canceled_=nullptr;
    Clock::time_point deadline_{};
    bool used_=false,waiting_=false,closed_=false,decided_=false,approved_=false;
};
}
