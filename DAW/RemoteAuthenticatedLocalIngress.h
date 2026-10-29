#pragma once
// Native IPC ingress, not a socket listener. The owner must provision a fresh
// OS-protected random secret and independently verify TLS identity/host consent.
// Native protocol: 4-byte BE payload length, then 64 lowercase hex secret bytes,
// newline, and the existing command wire. This is distinct from Python JSON IPC.
#include "RemoteVerifiedLocalCommandQueueAdapter.h"
#include "RemoteWireFrame.h"
#include <mutex>

namespace idw::daw::remote {
class AuthenticatedLocalIngress {
public:
    explicit AuthenticatedLocalIngress(TrustedTransportEventQueue& queue)
        : queue_(queue), adapter_(queue) {}

    // Trusted owner API only; these facts must never come from socket fields.
    bool beginSession(std::string secret, std::uint64_t epoch,
                      bool tlsVerified, bool hostConsented, bool trackControlGranted) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_ || epoch <= highestEpoch_ || !validSecret(secret) ||
            !tlsVerified || !hostConsented || !trackControlGranted) return false;
        if (!queue_.push({TrustedTransportEvent::Kind::connected, epoch,
                          true, true, true, {}})) return false;
        secret_ = std::move(secret);
        epoch_ = highestEpoch_ = epoch;
        highestSequence_ = 0;
        active_ = true;
        return true;
    }

    // One complete bounded frame per call. A future socket worker must assemble
    // partial reads with StreamDecoder, enforce deadlines, and bind loopback only.
    // Serialization preserves replay state across connections and revocation.
    bool receiveFrame(const std::vector<std::uint8_t>& frame) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || frame.size() > maxControlPayload + 4) return false;
        std::string payload;
        if (decodeFrame(frame, payload) != FrameResult::complete ||
            payload.size() <= 65 || payload[64] != '\n') return false;
        // Fixed-work comparison for the fixed-size secret; no early mismatch exit.
        volatile unsigned difference = 0;
        for (std::size_t i = 0; i < 64; ++i)
            difference = difference | (static_cast<unsigned char>(payload[i]) ^
                                      static_cast<unsigned char>(secret_[i]));
        if (difference != 0) return false;
        const auto wire = payload.substr(65);
        CommandMessage parsed;
        if (!parseControlCommand(wire, parsed) || parsed.sessionEpoch != epoch_ ||
            parsed.sequence <= highestSequence_) return false;
        if (!adapter_.submitValidatedCommand(wire, epoch_)) return false;
        highestSequence_ = parsed.sequence;
        return true; // Queued for host review, never approval or execution.
    }

    void endSession(std::uint64_t expectedEpoch = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || (expectedEpoch != 0 && expectedEpoch != epoch_)) return;
        active_ = false;
        secret_.clear();
        queue_.push({TrustedTransportEvent::Kind::authenticationLost, epoch_,
                     false, false, false, {}});
    }

private:
    static bool validSecret(const std::string& secret) {
        if (secret.size() != 64) return false;
        for (char c : secret)
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
        return true; // Syntax only; entropy is the trusted owner's responsibility.
    }
    TrustedTransportEventQueue& queue_;
    VerifiedLocalCommandQueueAdapter adapter_;
    std::mutex mutex_;
    std::string secret_;
    std::uint64_t epoch_ = 0, highestEpoch_ = 0, highestSequence_ = 0;
    bool active_ = false;
};
} // namespace idw::daw::remote
