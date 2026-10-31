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
               const std::string& peerSha256,unsigned timeoutMs=2000) {
        if(operation_ || generation_==std::numeric_limits<std::uint64_t>::max() || epoch==0 ||
           !validBootstrapHex(imageSha256) || !validBootstrapHex(peerSha256) || timeoutMs<20 || timeoutMs>5000) return false;
        auto op=std::make_shared<Operation>(++generation_);
        operation_=op;
        try {
            worker_=std::thread([this,op,executable,imageSha256,options,epoch,peerSha256,timeoutMs] {
                try {
                    op->success=manager_.startWithOffer(executable,imageSha256,options,epoch,peerSha256,
                        [op,timeoutMs](const LocalTLSOffer& offer,LocalHostConsent& consent,const std::atomic<bool>& cancel) {
                            return !op->canceled.load() && op->consent.request(offer,consent,cancel,
                                std::chrono::milliseconds(timeoutMs));
                        },timeoutMs,&op->canceled);
                } catch(...) {op->success=false;}
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
    struct Operation {
        explicit Operation(std::uint64_t value):generation(value){}
        const std::uint64_t generation;
        LocalOfferConsent consent;
        std::atomic<bool> canceled{false},done{false};
        bool success=false,reported=false;
    };
    Manager& manager_; // Must outlive controller; its event queue also remains alive.
    std::shared_ptr<Operation> operation_;
    std::thread worker_;
    std::uint64_t generation_=0;
};
}
