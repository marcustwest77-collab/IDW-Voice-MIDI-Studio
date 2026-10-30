#pragma once
#include "RemoteLocalSessionOwner.h"
#include <memory>

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
    bool onTransportLost(const LocalTransportBinding&);
    void stop();
    bool running() const;
    std::uint32_t processId() const; // Diagnostic only; never used as authentication.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
