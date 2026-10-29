#pragma once
#include "RemoteAuthenticatedLocalIngress.h"
#include <memory>

namespace idw::daw::remote {
// Explicitly started development listener. No UI activation or provisioning.
// Trusted owner must provide transport-bound consent and an OS-protected secret.
class LocalSocketReceiver {
public:
    struct Options {
        std::uint16_t port = 0; // OS-assigned by default; IPv4 loopback only.
        unsigned frameTimeoutMs = 1000; // Total deadline, not per-read timeout.
        unsigned sessionTimeoutMs = 60000;
        unsigned maxConnections = 128; // Includes rejected/empty connections.
    };
    explicit LocalSocketReceiver(AuthenticatedLocalIngress& ingress);
    ~LocalSocketReceiver();
    LocalSocketReceiver(const LocalSocketReceiver&) = delete;
    LocalSocketReceiver& operator=(const LocalSocketReceiver&) = delete;
    bool start(const Options&, std::string secret, std::uint64_t epoch,
               bool tlsVerified, bool hostConsented, bool trackControlGranted);
    // Revokes ingress, interrupts bounded polling, joins worker, closes listener.
    // Call stop before restarting after automatic expiry/connection exhaustion.
    void stop();
    bool running() const;
    std::uint16_t port() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace idw::daw::remote
