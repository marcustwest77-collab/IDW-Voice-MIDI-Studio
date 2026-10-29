#pragma once
#include "RemoteLocalSessionOwner.h"
#include <memory>

namespace idw::daw::remote {
// Windows-only, single-use local bootstrap channel. No launch or auto activation.
// Process handles must be held open, trusted owner-selected handles with query
// and SYNCHRONIZE rights; numeric PIDs and pipe names are not identity proofs.
class WindowsBootstrapChannel {
public:
    WindowsBootstrapChannel();
    ~WindowsBootstrapChannel();
    WindowsBootstrapChannel(const WindowsBootstrapChannel&) = delete;
    WindowsBootstrapChannel& operator=(const WindowsBootstrapChannel&) = delete;
    bool create();
    std::wstring name() const; // Non-secret discovery path; never contains token.
    bool deliverTo(void* expectedClientProcess, const LocalSessionBootstrap&,
                   unsigned timeoutMs = 2000);
    void cancel(); // Safe while deliverTo is waiting; no raw handle is exposed.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
// Verify the server against the trusted held process handle before reading.
// The bounded, synchronous consumer must validate/use the binding before true.
// It must not log secrets, retain the reference, or block indefinitely.
bool receiveWindowsBootstrap(const std::wstring& name, void* expectedServerProcess,
                             const LocalSessionOwner::Provisioner& consume,
                             unsigned timeoutMs = 2000);
}
