#pragma once
#include "RemoteLocalSessionOwner.h"
#include <memory>
#include <atomic>
#include <functional>
#include <chrono>
#include "RemoteTLSOfferWire.h"

namespace idw::daw::remote {
// Diagnostic hash calculation only. Its result is NOT automatic approval of an
// executable. The launch pin must come from a separately trusted manifest/policy.
bool inspectWindowsBridgeSha256(const std::wstring& path, std::string& output);

class WindowsBridgeSession {
public:
    explicit WindowsBridgeSession(TrustedTransportEventQueue& queue);
    ~WindowsBridgeSession();
    WindowsBridgeSession(const WindowsBridgeSession&) = delete;
    WindowsBridgeSession& operator=(const WindowsBridgeSession&) = delete;
    // Explicit trusted local executable/pin and real TLS-owner evidence required.
    // No shell, PATH lookup, arbitrary arguments or default executable selection.
    bool start(const std::wstring& executable, const std::string& approvedSha256,
               const LocalSocketReceiver::Options&, const LocalTransportBinding&,
               const LocalHostConsent&, unsigned bootstrapTimeoutMs = 2000);
    // Launch before TLS offer, then ask a trusted bounded local controller for
    // exact-binding consent. Run off the UI thread. Callback must not re-enter
    // this manager or retain references and must honor cancellation. No peer
    // message may supply this callback or its host consent decision.
    using OfferConsent=std::function<bool(const LocalTLSOffer&,LocalHostConsent&,const std::atomic<bool>&)>;
    bool startWithOffer(const std::wstring& executable,const std::string& approvedSha256,
                       const LocalSocketReceiver::Options&,std::uint64_t expectedEpoch,
                       const std::string& approvedPeerSha256,const OfferConsent&,
                       unsigned timeoutMs=2000,const std::atomic<bool>* launchCanceled=nullptr);
    // launchCanceled, when supplied, must outlive startWithOffer. It closes the
    // pre-entry cancellation race for asynchronous owners; stop() still performs
    // cancellation/cleanup once the manager has started processing the request.
    using OfferDeadline=std::chrono::steady_clock::time_point;
    using TimedOfferConsent=std::function<bool(const LocalTLSOffer&,LocalHostConsent&,const std::atomic<bool>&,OfferDeadline)>;
    // Explicit managed UI budget up to 30 seconds, with the private channel's
    // exact shared deadline supplied to consent. Legacy startWithOffer stays <=5s.
    bool startWithTimedOffer(const std::wstring&,const std::string&,const LocalSocketReceiver::Options&,
                             std::uint64_t,const std::string&,const TimedOfferConsent&,
                             unsigned timeoutMs=30000,const std::atomic<bool>* launchCanceled=nullptr);
    bool onTransportLost(const LocalTransportBinding&);
    void stop();
    bool running() const;
    std::uint32_t processId() const; // Diagnostic only; never used as authentication.
private:
    bool startWithOfferImpl(const std::wstring&,const std::string&,const LocalSocketReceiver::Options&,
                            std::uint64_t,const std::string&,const TimedOfferConsent&,unsigned,
                            const std::atomic<bool>*,std::uint64_t stopSequence);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
