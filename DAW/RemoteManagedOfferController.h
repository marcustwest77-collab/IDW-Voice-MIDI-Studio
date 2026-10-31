#pragma once
// One trusted UI/controller thread owns start/pending/resolve/result/stop calls.
// Background work touches only its operation and manager, never JUCE components.
#include "RemoteWindowsBridgeSession.h"
#include "RemoteLocalOfferConsent.h"
#include <limits>
#include <thread>

namespace idw::daw::remote {
template<class Manager=WindowsBridgeSession>
class ManagedOfferController {
public:
    explicit ManagedOfferController(Manager& manager):manager_(manager){}
    ~ManagedOfferController(){stop();}
    ManagedOfferController(const ManagedOfferController&)=delete;
    ManagedOfferController& operator=(const ManagedOfferController&)=delete;
    bool busy() const {return bool(operation_);}
    // True means startup was scheduled, never that TLS/native admission succeeded.
    // Caller supplies independently trusted artifact/peer policy. No defaults.
    bool start(const std::wstring& executable,const std::string& imageSha256,
               const LocalSocketReceiver::Options& options,std::uint64_t epoch,
               const std::string& peerSha256,unsigned timeoutMs=2000,bool readinessProfile=false) {
        if(operation_ || generation_==std::numeric_limits<std::uint64_t>::max() || epoch==0 ||
           !validBootstrapHex(imageSha256) || !validBootstrapHex(peerSha256) || timeoutMs<20 || timeoutMs>30000) return false;
        auto op=std::make_shared<Operation>(++generation_);
        operation_=op;
        try {
            worker_=std::thread([this,op,executable,imageSha256,options,epoch,peerSha256,timeoutMs,readinessProfile] {
                try {
                    const auto consentFor=[op](const LocalTLSOffer& offer,LocalHostConsent& consent,const std::atomic<bool>& cancel,
                             WindowsBridgeSession::OfferDeadline deadline) {
                            return !op->canceled.load() && op->consent.requestUntil(offer,consent,cancel,deadline);
                        };
                    if(readinessProfile) {
                        op->success=manager_.startWithReadyOffer(executable,imageSha256,options,epoch,peerSha256,
                            [op,epoch](const LocalBridgeReady& ready,const std::atomic<bool>& cancel,WindowsBridgeSession::OfferDeadline deadline) {
                                if(op->canceled.load() || cancel.load() || ready.epoch!=epoch || ready.tlsPort==0 ||
                                   std::chrono::steady_clock::now()>=deadline) return false;
                                std::atomic_store(&op->ready,std::make_shared<const Readiness>(Readiness{ready,deadline}));return true;
                            },consentFor,timeoutMs,&op->canceled);
                    } else op->success=manager_.startWithTimedOffer(executable,imageSha256,options,epoch,peerSha256,
                        consentFor,timeoutMs,&op->canceled);
                } catch(...) {op->success=false;}
                std::atomic_store(&op->ready,std::shared_ptr<const Readiness>{});
                op->done.store(true,std::memory_order_release);
            });
        } catch(...) {operation_.reset();return false;}
        return true;
    }
    std::optional<LocalOfferPrompt> pending() const {
        if(!operation_) return std::nullopt;
        const auto offer=operation_->consent.pending();
        if(!offer) return std::nullopt;
        return LocalOfferPrompt{operation_->generation,*offer};
    }
    // Non-secret discovery only, a live snapshot for the owned operation.
    // Only while startup is pending and its original deadline remains live.
    // The caller must still verify TLS; this is never native control authority.
    std::optional<LocalBridgeReady> pendingReadiness() const {
        if(!operation_ || operation_->canceled.load() ||
           operation_->done.load(std::memory_order_acquire)) return std::nullopt;
        const auto ready=std::atomic_load(&operation_->ready);
        if(!ready || std::chrono::steady_clock::now()>=ready->deadline ||
           operation_->done.load(std::memory_order_acquire)) return std::nullopt;
        return ready->value;
    }
    // One-shot consumer retained for drivers; it does not hide the live UI state.
    std::optional<LocalBridgeReady> takeReadiness() {
        if(!operation_ || operation_->readyReported) return std::nullopt;
        const auto ready=pendingReadiness();
        if(ready) operation_->readyReported=true;
        return ready;
    }
    bool resolve(const LocalOfferPrompt& reviewed,bool allowTrackControl) {
        return operation_ && reviewed.generation==operation_->generation &&
            operation_->consent.resolve(reviewed.offer,allowTrackControl);
    }
    // One diagnostic startup result, not live authority or execution permission.
    std::optional<bool> takeStartupResult() {
        if(!operation_ || !operation_->done.load(std::memory_order_acquire) || operation_->reported) return std::nullopt;
        operation_->reported=true;return operation_->success;
    }
    void stop() {
        if(operation_) {operation_->canceled.store(true);operation_->consent.close();}
        manager_.stop();
        if(worker_.joinable()) worker_.join();
        operation_.reset();
    }
private:
    struct Readiness {
        LocalBridgeReady value;
        WindowsBridgeSession::OfferDeadline deadline;
    };
    struct Operation {
        explicit Operation(std::uint64_t value):generation(value){}
        const std::uint64_t generation;
        LocalOfferConsent consent;
        std::shared_ptr<const Readiness> ready;
        std::atomic<bool> canceled{false},done{false};
        bool success=false,reported=false,readyReported=false;
    };
    Manager& manager_; // Must outlive controller; its event queue also remains alive.
    std::shared_ptr<Operation> operation_;
    std::thread worker_;
    std::uint64_t generation_=0;
};
}
