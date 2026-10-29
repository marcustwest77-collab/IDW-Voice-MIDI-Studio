#pragma once
#include "RemoteLocalSessionSecret.h"
#include "RemoteLocalSocketReceiver.h"
#include <functional>

namespace idw::daw::remote {
// These values are trusted TLS-owner evidence, never fields from socket input.
// This type cannot itself verify a certificate or establish a channel binding.
struct LocalTransportBinding {
    std::uint64_t epoch = 0;
    std::string peerSha256;
    std::string connectionId; // fresh ID of the verified transport connection
    bool tlsVerified = false;
};
struct LocalHostConsent {
    std::uint64_t epoch = 0;
    std::string peerSha256;
    std::string connectionId;
    bool hostApproved = false;
    bool trackControlGranted = false;
};
struct LocalSessionBootstrap {
    std::uint16_t port = 0;
    std::uint64_t epoch = 0;
    std::string secret;
    std::string peerSha256;
    std::string connectionId;
};

class LocalSessionOwner {
public:
    // Synchronous trusted in-process handoff. Must not retain the reference,
    // block indefinitely, log, or expose the secret to peers. Called without
    // the owner mutex so transport loss/host stop can interrupt provisioning.
    // Cross-process handoff must use a protected channel supplied by its owner.
    using Provisioner = std::function<bool(const LocalSessionBootstrap&)>;
    explicit LocalSessionOwner(TrustedTransportEventQueue& queue)
        : ingress_(queue), receiver_(ingress_) {}
    ~LocalSessionOwner() { stop(); }

    bool start(const LocalSocketReceiver::Options& options,
               const LocalTransportBinding& transport, const LocalHostConsent& consent,
               const Provisioner& provision) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (owned_ || !provision || !transport.tlsVerified || transport.epoch == 0 ||
            transport.epoch <= highestEpoch_ || !hexId(transport.peerSha256) ||
            !hexId(transport.connectionId) || !consent.hostApproved ||
            !consent.trackControlGranted || consent.epoch != transport.epoch ||
            consent.peerSha256 != transport.peerSha256 ||
            consent.connectionId != transport.connectionId) return false;
        LocalSessionBootstrap bootstrap;
        if (!generateLocalSessionSecret(bootstrap.secret)) return false;
        bootstrap.epoch = transport.epoch;
        bootstrap.peerSha256 = transport.peerSha256;
        bootstrap.connectionId = transport.connectionId;
        LocalTransportBinding admitted = transport; // Allocate before opening a listener.
        if (!receiver_.start(options, bootstrap.secret, transport.epoch, true, true, true)) return false;
        // Even failed provisioning consumes its epoch: no half-provisioned retry.
        highestEpoch_ = transport.epoch;
        binding_ = std::move(admitted);
        owned_ = true;
        bootstrap.port = receiver_.port();
        const auto startedEpoch = transport.epoch;
        lock.unlock();
        bool provisioned = false;
        try { provisioned = provision(bootstrap); }
        catch (...) { provisioned = false; }
        lock.lock();
        // A canceled handoff must never stop or report success for a newer owner.
        if (!owned_ || binding_.epoch != startedEpoch) return false;
        if (!provisioned || !receiver_.running()) {
            stopLocked();
            return false;
        }
        return true;
    }
    // A delayed loss/revocation callback from an old connection cannot stop a
    // newly admitted connection, even when it names the same peer certificate.
    bool onTransportLost(const LocalTransportBinding& lost) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!owned_ || lost.epoch != binding_.epoch ||
            lost.peerSha256 != binding_.peerSha256 ||
            lost.connectionId != binding_.connectionId) return false;
        stopLocked();
        return true;
    }
    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopLocked();
    }
    bool running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return owned_ && receiver_.running();
    }
    std::uint16_t port() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return owned_ ? receiver_.port() : 0;
    }
private:
    static bool hexId(const std::string& id) {
        if (id.size() != 64) return false;
        for (char c : id)
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        return true;
    }
    void stopLocked() {
        receiver_.stop();
        binding_ = {};
        owned_ = false;
    }
    mutable std::mutex mutex_;
    AuthenticatedLocalIngress ingress_;
    LocalSocketReceiver receiver_;
    LocalTransportBinding binding_;
    std::uint64_t highestEpoch_ = 0;
    bool owned_ = false;
};
} // namespace idw::daw::remote
