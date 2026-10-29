#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#endif
#include "RemoteWindowsBootstrapChannel.h"
#include "RemoteLocalBootstrapWire.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

namespace idw::daw::remote {
#if defined(_WIN32)
namespace {
using Clock = std::chrono::steady_clock;
constexpr wchar_t prefix[] = L"\\\\.\\pipe\\IDW-LocalBootstrap-";
constexpr char receipt[] = "IDW_BOOTSTRAP_OK\n";
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { reset(); }
    void reset(HANDLE next = INVALID_HANDLE_VALUE) {
        if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
        value = next;
    }
};
struct LocalMemory {
    void* value = nullptr;
    ~LocalMemory() { if (value) LocalFree(value); }
};
bool alive(HANDLE process) {
    return process && process != INVALID_HANDLE_VALUE &&
        WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
}
bool allowedName(const std::wstring& name) {
    const std::wstring start(prefix);
    if (name.size() != start.size() + 64 || name.compare(0, start.size(), start) != 0) return false;
    for (std::size_t i = start.size(); i < name.size(); ++i)
        if (!((name[i] >= L'0' && name[i] <= L'9') || (name[i] >= L'a' && name[i] <= L'f'))) return false;
    return true;
}
bool privateDescriptor(LocalMemory& descriptor) {
    Handle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value)) return false;
    DWORD size = 0;
    GetTokenInformation(token.value, TokenUser, nullptr, 0, &size);
    if (size == 0) return false;
    std::vector<unsigned char> data(size);
    if (!GetTokenInformation(token.value, TokenUser, data.data(), size, &size)) return false;
    LocalMemory sid;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,
                               reinterpret_cast<LPWSTR*>(&sid.value))) return false;
    const auto sddl = std::wstring(L"D:P(A;;GA;;;") + static_cast<wchar_t*>(sid.value) + L")";
    return ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1,
        reinterpret_cast<PSECURITY_DESCRIPTOR*>(&descriptor.value), nullptr) != FALSE;
}
// Every operation uses overlapped I/O and one shared absolute deadline.
bool finish(HANDLE pipe, OVERLAPPED& operation, Clock::time_point deadline,
            HANDLE peer, const std::atomic<bool>& canceled, DWORD& transferred) {
    while (!canceled.load() && alive(peer) && Clock::now() < deadline) {
        const DWORD status = WaitForSingleObject(operation.hEvent, 10);
        if (status == WAIT_OBJECT_0) return GetOverlappedResult(pipe, &operation, &transferred, FALSE) != FALSE;
        if (status != WAIT_TIMEOUT) break;
    }
    CancelIoEx(pipe, &operation);
    // Drain cancellation before destroying the OVERLAPPED/event/buffer.
    GetOverlappedResult(pipe, &operation, &transferred, TRUE);
    return false;
}
bool transfer(HANDLE pipe, bool writing, unsigned char* bytes, std::size_t size,
              Clock::time_point deadline, HANDLE peer, const std::atomic<bool>& canceled) {
    std::size_t offset = 0;
    while (offset < size && !canceled.load() && alive(peer) && Clock::now() < deadline) {
        Handle event{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
        if (!event.value) return false;
        OVERLAPPED operation{};
        operation.hEvent = event.value;
        DWORD count = 0;
        const auto remaining = static_cast<DWORD>(size - offset);
        const BOOL done = writing ? WriteFile(pipe, bytes + offset, remaining, &count, &operation)
                                  : ReadFile(pipe, bytes + offset, remaining, &count, &operation);
        if (!done && (GetLastError() != ERROR_IO_PENDING ||
                      !finish(pipe, operation, deadline, peer, canceled, count))) return false;
        if (count == 0) return false;
        offset += count;
    }
    return offset == size && !canceled.load() && Clock::now() < deadline;
}
bool connectClient(HANDLE pipe, Clock::time_point deadline, HANDLE peer,
                   const std::atomic<bool>& canceled) {
    Handle event{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    if (!event.value) return false;
    OVERLAPPED operation{};
    operation.hEvent = event.value;
    if (ConnectNamedPipe(pipe, &operation)) return true;
    const auto error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) return true;
    DWORD unused = 0;
    return error == ERROR_IO_PENDING && finish(pipe, operation, deadline, peer, canceled, unused);
}
}
#endif

struct WindowsBootstrapChannel::Impl {
    mutable std::mutex mutex;
    std::atomic<bool> canceled{false};
    std::wstring name;
#if defined(_WIN32)
    Handle pipe;
#endif
};
WindowsBootstrapChannel::WindowsBootstrapChannel() : impl_(std::make_unique<Impl>()) {}
WindowsBootstrapChannel::~WindowsBootstrapChannel() { cancel(); }
bool WindowsBootstrapChannel::create() {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.mutex);
#if defined(_WIN32)
    if (p.pipe.value != INVALID_HANDLE_VALUE) return false;
    std::string nonce;
    if (!generateLocalSessionSecret(nonce)) return false;
    LocalMemory descriptor;
    if (!privateDescriptor(descriptor)) return false;
    const auto path = std::wstring(prefix) + std::wstring(nonce.begin(), nonce.end());
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor.value, FALSE};
    p.pipe.reset(CreateNamedPipeW(path.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
        FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT |
        PIPE_REJECT_REMOTE_CLIENTS, 1, 4096, 4096, 0, &security));
    if (p.pipe.value == INVALID_HANDLE_VALUE) return false;
    p.name = path;
    p.canceled.store(false);
    return true;
#else
    return false; // No insecure fallback on other platforms.
#endif
}
std::wstring WindowsBootstrapChannel::name() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->name;
}
void WindowsBootstrapChannel::cancel() { impl_->canceled.store(true); }
bool WindowsBootstrapChannel::deliverTo(void* expectedClientProcess,
        const LocalSessionBootstrap& bootstrap, unsigned timeoutMs) {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.mutex);
#if defined(_WIN32)
    // Close the single-use instance on every outcome, including validation failure.
    struct CloseOnReturn { Handle& pipe; ~CloseOnReturn() { pipe.reset(); } } cleanup{p.pipe};
    const auto peer = static_cast<HANDLE>(expectedClientProcess);
    LocalBootstrapBytes bytes{};
    if (p.pipe.value == INVALID_HANDLE_VALUE || p.canceled.load() || timeoutMs < 20 || timeoutMs > 5000 ||
        !alive(peer) || GetProcessId(peer) == 0 || GetProcessId(peer) == GetCurrentProcessId() ||
        !encodeLocalBootstrap(bootstrap, bytes)) return false;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    for (unsigned attempt = 0; attempt < 8 && !p.canceled.load(); ++attempt) {
        if (!connectClient(p.pipe.value, deadline, peer, p.canceled)) return false;
        ULONG pid = 0;
        if (GetNamedPipeClientProcessId(p.pipe.value, &pid) && pid == GetProcessId(peer) && alive(peer)) {
            if (!transfer(p.pipe.value, true, bytes.data(), bytes.size(), deadline, peer, p.canceled)) return false;
            std::array<unsigned char, sizeof(receipt) - 1> ack{};
            if (!transfer(p.pipe.value, false, ack.data(), ack.size(), deadline, peer, p.canceled)) return false;
            return std::string(ack.begin(), ack.end()) == receipt;
        }
        DisconnectNamedPipe(p.pipe.value); // Wrong PID receives no secret bytes.
    }
    return false;
#else
    (void)expectedClientProcess; (void)bootstrap; (void)timeoutMs;
    return false;
#endif
}
bool receiveWindowsBootstrap(const std::wstring& name, void* expectedServerProcess,
        const LocalSessionOwner::Provisioner& consume, unsigned timeoutMs) {
#if defined(_WIN32)
    const auto peer = static_cast<HANDLE>(expectedServerProcess);
    if (!consume || !allowedName(name) || timeoutMs < 20 || timeoutMs > 5000 ||
        !alive(peer) || GetProcessId(peer) == 0) return false;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    Handle pipe;
    while (Clock::now() < deadline && alive(peer)) {
        pipe.reset(CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
        if (pipe.value != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_PIPE_BUSY && GetLastError() != ERROR_FILE_NOT_FOUND) return false;
        Sleep(10);
    }
    ULONG pid = 0;
    if (pipe.value == INVALID_HANDLE_VALUE || !GetNamedPipeServerProcessId(pipe.value, &pid) ||
        pid != GetProcessId(peer) || !alive(peer)) return false;
    const std::atomic<bool> canceled{false};
    LocalBootstrapBytes bytes{};
    LocalSessionBootstrap bootstrap;
    if (!transfer(pipe.value, false, bytes.data(), bytes.size(), deadline, peer, canceled) ||
        !decodeLocalBootstrap(bytes, bootstrap)) return false;
    try { if (!consume(bootstrap)) return false; }
    catch (...) { return false; }
    auto ack = std::string(receipt);
    return transfer(pipe.value, true, reinterpret_cast<unsigned char*>(&ack[0]), ack.size(), deadline, peer, canceled);
#else
    (void)name; (void)expectedServerProcess; (void)consume; (void)timeoutMs;
    return false;
#endif
}
}
