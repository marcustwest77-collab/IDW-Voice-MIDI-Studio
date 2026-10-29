#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
#include "RemoteLocalSocketReceiver.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace idw::daw::remote {
namespace {
using Clock = std::chrono::steady_clock;
#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket invalidSocket = INVALID_SOCKET;
using AddressLength = int;
void closeSocket(Socket s) { closesocket(s); }
bool nonblocking(Socket s) { u_long one = 1; return ioctlsocket(s, FIONBIO, &one) == 0; }
bool retryable() { const int e = WSAGetLastError(); return e == WSAEWOULDBLOCK || e == WSAEINTR; }
#else
using Socket = int;
constexpr Socket invalidSocket = -1;
using AddressLength = socklen_t;
void closeSocket(Socket s) { close(s); }
bool nonblocking(Socket s) {
    const int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}
bool retryable() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
#endif
struct OwnedSocket {
    Socket value = invalidSocket;
    ~OwnedSocket() { if (value != invalidSocket) closeSocket(value); }
};
// No reuse options: on Windows explicitly prohibit competing binds.
bool exclusive(Socket s) {
#if defined(_WIN32)
    const BOOL enabled = TRUE;
    return setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                     reinterpret_cast<const char*>(&enabled), sizeof(enabled)) == 0;
#else
    (void)s;
    return true;
#endif
}
bool waitReadable(Socket s, Clock::time_point deadline, const std::atomic<bool>& stopping) {
#if !defined(_WIN32)
    if (s < 0 || s >= FD_SETSIZE) return false;
#endif
    while (!stopping.load() && Clock::now() < deadline) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(s, &readSet);
        timeval timeout{};
        timeout.tv_usec = 10000; // stop/join remains responsive, including idle clients.
#if defined(_WIN32)
        const int result = select(0, &readSet, nullptr, nullptr, &timeout);
#else
        const int result = select(s + 1, &readSet, nullptr, nullptr, &timeout);
#endif
        if (result > 0) return !stopping.load() && Clock::now() < deadline;
        if (result < 0 && !retryable()) return false;
    }
    return false;
}
bool readExact(Socket s, std::uint8_t* bytes, std::size_t count,
               Clock::time_point deadline, const std::atomic<bool>& stopping) {
    std::size_t received = 0;
    while (received < count) {
        if (!waitReadable(s, deadline, stopping)) return false;
        const auto n = recv(s, reinterpret_cast<char*>(bytes + received),
                            static_cast<int>(count - received), 0);
        if (n > 0) received += static_cast<std::size_t>(n);
        else if (n == 0 || !retryable()) return false;
    }
    return true;
}
} // namespace

struct LocalSocketReceiver::Impl {
    explicit Impl(AuthenticatedLocalIngress& i) : ingress(i) {
#if defined(_WIN32)
        WSADATA data{};
        networkReady = WSAStartup(MAKEWORD(2, 2), &data) == 0;
#endif
    }
    ~Impl() {
#if defined(_WIN32)
        if (networkReady) WSACleanup();
#endif
    }
    AuthenticatedLocalIngress& ingress;
    std::mutex lifecycleMutex;
    std::thread worker;
    std::atomic<bool> stopping{false}, active{false};
    std::atomic<std::uint16_t> boundPort{0};
    Socket listener = invalidSocket; // closed only after worker is joined
    std::uint64_t sessionEpoch = 0;
    bool networkReady = true;

    void handle(Socket client, Clock::time_point deadline) {
        if (!nonblocking(client)) return;
        std::vector<std::uint8_t> frame(4);
        if (!readExact(client, frame.data(), 4, deadline, stopping)) return;
        const auto size = (std::uint32_t(frame[0]) << 24) |
            (std::uint32_t(frame[1]) << 16) | (std::uint32_t(frame[2]) << 8) | frame[3];
        if (size == 0 || size > maxControlPayload) return;
        frame.resize(4 + size);
        if (!readExact(client, frame.data() + 4, size, deadline, stopping)) return;
        // Exactly one frame per connection. Require write-half-close, rejecting
        // trailing bytes before queue insertion, even when split across reads.
        if (!waitReadable(client, deadline, stopping)) return;
        char extra{};
        if (recv(client, &extra, 1, 0) != 0 || stopping.load() || Clock::now() >= deadline) return;
        const bool accepted = ingress.receiveFrame(frame);
        const char receipt = accepted ? '1' : '0'; // queue receipt only
        // Best-effort nonblocking receipt. Never log frames or session secrets.
#if defined(MSG_NOSIGNAL)
        send(client, &receipt, 1, MSG_NOSIGNAL);
#elif defined(SO_NOSIGPIPE)
        int one = 1;
        if (setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one)) == 0)
            send(client, &receipt, 1, 0);
#else
        send(client, &receipt, 1, 0);
#endif
    }
    void run(Options options) noexcept {
        try {
            const auto sessionDeadline = Clock::now() + std::chrono::milliseconds(options.sessionTimeoutMs);
            for (unsigned count = 0; count < options.maxConnections;) {
                if (!waitReadable(listener, sessionDeadline, stopping)) break;
                sockaddr_in peer{};
                AddressLength length = sizeof(peer);
                OwnedSocket client{accept(listener, reinterpret_cast<sockaddr*>(&peer), &length)};
                if (client.value == invalidSocket) {
                    if (retryable()) continue;
                    break;
                }
                ++count;
                if (peer.sin_family != AF_INET || ntohl(peer.sin_addr.s_addr) != INADDR_LOOPBACK) continue;
                handle(client.value, std::min(sessionDeadline,
                    Clock::now() + std::chrono::milliseconds(options.frameTimeoutMs)));
            }
        } catch (...) { /* Fail closed, including allocation failure. */ }
        ingress.endSession(sessionEpoch);
        closeSocket(listener);
        listener = invalidSocket;
        boundPort.store(0);
        active.store(false);
    }
};

LocalSocketReceiver::LocalSocketReceiver(AuthenticatedLocalIngress& ingress)
    : impl_(std::make_unique<Impl>(ingress)) {}
LocalSocketReceiver::~LocalSocketReceiver() { stop(); }
bool LocalSocketReceiver::start(const Options& options, std::string secret,
                               std::uint64_t epoch, bool tlsVerified,
                               bool hostConsented, bool trackControlGranted) {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.lifecycleMutex);
    if (!p.networkReady || p.worker.joinable() || options.frameTimeoutMs < 20 ||
        options.frameTimeoutMs > 5000 || options.sessionTimeoutMs < options.frameTimeoutMs ||
        options.sessionTimeoutMs > 300000 || options.maxConnections == 0 ||
        options.maxConnections > 1024) return false;
    OwnedSocket listener{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (listener.value == invalidSocket || !exclusive(listener.value) || !nonblocking(listener.value)) return false;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // No caller-controlled bind address.
    address.sin_port = htons(options.port);
    if (bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(listener.value, 4) != 0) return false;
    AddressLength length = sizeof(address);
    if (getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &length) != 0) return false;
    if (!p.ingress.beginSession(std::move(secret), epoch, tlsVerified, hostConsented, trackControlGranted)) return false;
    p.listener = listener.value;
    p.sessionEpoch = epoch;
    listener.value = invalidSocket;
    p.stopping.store(false);
    p.active.store(true);
    p.boundPort.store(ntohs(address.sin_port));
    try { p.worker = std::thread([&p, options] { p.run(options); }); }
    catch (...) {
        p.ingress.endSession(p.sessionEpoch);
        closeSocket(p.listener);
        p.listener = invalidSocket;
        p.boundPort.store(0);
        p.active.store(false);
        return false;
    }
    return true;
}
void LocalSocketReceiver::stop() {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.lifecycleMutex);
    p.stopping.store(true);
    // This receiver may only revoke an admission it owns.
    if (p.worker.joinable()) {
        p.ingress.endSession(p.sessionEpoch);
        p.worker.join();
    }
    if (p.listener != invalidSocket) { closeSocket(p.listener); p.listener = invalidSocket; }
    p.boundPort.store(0);
    p.active.store(false);
}
bool LocalSocketReceiver::running() const { return impl_->active.load(); }
std::uint16_t LocalSocketReceiver::port() const { return impl_->boundPort.load(); }
} // namespace idw::daw::remote
