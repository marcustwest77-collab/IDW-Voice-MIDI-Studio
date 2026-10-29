// Synthetic TLS-owner evidence; this executable does not authenticate TLS peers.
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
#include <unistd.h>
#endif
#include "../RemoteLocalSessionOwner.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <thread>

using namespace idw::daw::remote;
bool deliver(const LocalSessionBootstrap& bootstrap, unsigned sequence) {
    std::vector<std::uint8_t> frame;
    assert(encodeFrame(bootstrap.secret + "\n1|" + std::to_string(bootstrap.epoch) +
        "|" + std::to_string(sequence) + "|test_request|arm|1", frame));
#if defined(_WIN32)
    const SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(client != INVALID_SOCKET);
#else
    const int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(client >= 0);
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(bootstrap.port);
    assert(connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    std::size_t sent = 0;
    while (sent < frame.size()) {
        const auto n = send(client, reinterpret_cast<const char*>(frame.data() + sent),
                            static_cast<int>(frame.size() - sent), 0);
        assert(n > 0);
        sent += static_cast<std::size_t>(n);
    }
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(client, &reads);
    timeval timeout{};
    timeout.tv_sec = 2;
#if defined(_WIN32)
    assert(shutdown(client, SD_SEND) == 0);
    assert(select(0, &reads, nullptr, nullptr, &timeout) == 1);
#else
    assert(shutdown(client, SHUT_WR) == 0);
    assert(select(client + 1, &reads, nullptr, nullptr, &timeout) == 1);
#endif
    char receipt{};
    const bool accepted = recv(client, &receipt, 1, 0) == 1 && receipt == '1';
#if defined(_WIN32)
    closesocket(client);
#else
    close(client);
#endif
    return accepted;
}
int main() {
    std::string one, two;
    assert(generateLocalSessionSecret(one) && generateLocalSessionSecret(two));
    assert(one.size() == 64 && two.size() == 64 && one != two);
    for (char c : one) assert((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    TrustedTransportEventQueue queue;
    LocalSessionOwner owner(queue);
    LocalSocketReceiver::Options options;
    options.sessionTimeoutMs = 15000;
    LocalTransportBinding transport{45, std::string(64, 'a'), std::string(64, 'b'), true};
    LocalHostConsent consent{45, transport.peerSha256, transport.connectionId, true, true};
    LocalSessionBootstrap bootstrap;
    int provisionCalls = 0;
    auto provision = [&](const LocalSessionBootstrap& supplied) {
        ++provisionCalls;
        bootstrap = supplied; // In-process test client only; no files/IPC/logging.
        return true;
    };
    auto denied = consent;
    denied.hostApproved = false;
    assert(!owner.start(options, transport, denied, provision));
    denied = consent;
    denied.trackControlGranted = false;
    assert(!owner.start(options, transport, denied, provision));
    denied = consent;
    denied.epoch = 46;
    assert(!owner.start(options, transport, denied, provision));
    denied = consent;
    denied.peerSha256 = std::string(64, 'c');
    assert(!owner.start(options, transport, denied, provision));
    denied = consent;
    denied.connectionId = std::string(64, 'c');
    assert(!owner.start(options, transport, denied, provision));
    auto unverified = transport;
    unverified.tlsVerified = false;
    assert(!owner.start(options, unverified, consent, provision));
    unverified = transport;
    unverified.peerSha256 = "invalid";
    assert(!owner.start(options, unverified, consent, provision));
    assert(!owner.start(options, transport, consent, {}));
    assert(!owner.running() && owner.port() == 0 && provisionCalls == 0 && queue.drain().empty());
    // Failure/exception in the handoff closes the socket and rolls back queued admission.
    assert(!owner.start(options, transport, consent, [](const LocalSessionBootstrap&) { return false; }));
    auto events = queue.drain();
    assert(events.size() == 1 && events.front().kind == TrustedTransportEvent::Kind::authenticationLost);
    assert(!owner.start(options, transport, consent, provision)); // failed epoch consumed
    transport.epoch = consent.epoch = 46;
    assert(!owner.start(options, transport, consent, [](const LocalSessionBootstrap&) -> bool { throw 1; }));
    assert(!owner.running() && owner.port() == 0);
    queue.drain();
    transport.epoch = consent.epoch = 47;
    assert(owner.start(options, transport, consent, provision));
    assert(owner.running() && bootstrap.port == owner.port() && bootstrap.epoch == 47);
    assert(bootstrap.secret.size() == 64 && bootstrap.peerSha256 == transport.peerSha256);
    assert(bootstrap.connectionId == transport.connectionId && provisionCalls == 1);
    assert(!owner.start(options, transport, consent, provision));
    assert(deliver(bootstrap, 1));
    events = queue.drain();
    assert(events.size() == 2 && events.front().kind == TrustedTransportEvent::Kind::connected);
    assert(events.back().wire == "1|47|1|test_request|arm|1");
    assert(events.back().wire.find(bootstrap.secret) == std::string::npos);
    auto lost = transport;
    lost.connectionId = std::string(64, 'c');
    assert(!owner.onTransportLost(lost) && owner.running());
    lost = transport;
    lost.peerSha256 = std::string(64, 'c');
    assert(!owner.onTransportLost(lost));
    lost = transport;
    --lost.epoch;
    assert(!owner.onTransportLost(lost));
    assert(deliver(bootstrap, 2));
    assert(owner.onTransportLost(transport));
    assert(!owner.running() && owner.port() == 0);
    events = queue.drain();
    assert(events.size() == 1 && events.front().kind == TrustedTransportEvent::Kind::authenticationLost);
    const auto previous = bootstrap;
    const auto oldTransport = transport;
    transport.epoch = consent.epoch = 48;
    transport.connectionId = consent.connectionId = std::string(64, 'c');
    assert(owner.start(options, transport, consent, provision));
    assert(bootstrap.secret != previous.secret);
    auto staleSecret = bootstrap;
    staleSecret.secret = previous.secret;
    assert(!deliver(staleSecret, 1));
    assert(!owner.onTransportLost(oldTransport) && owner.running());
    assert(deliver(bootstrap, 1));
    std::thread disconnect([&] { owner.onTransportLost(transport); });
    owner.stop();
    disconnect.join();
    assert(!owner.running());
    events = queue.drain();
    assert(events.size() == 1 && events.front().epoch == 48);
    // A handoff taking longer than the listener lifetime cannot report success.
    auto shortLife = options;
    shortLife.frameTimeoutMs = 20;
    shortLife.sessionTimeoutMs = 40;
    transport.epoch = consent.epoch = 49;
    // Transport loss can cancel a handoff that is still waiting on its consumer.
    std::mutex barrier;
    std::condition_variable changed;
    bool entered = false, release = false, oldStartResult = true;
    const auto waitingTransport = transport;
    const auto waitingConsent = consent;
    std::thread waiting([&] {
        oldStartResult = owner.start(options, waitingTransport, waitingConsent,
            [&](const LocalSessionBootstrap&) {
                std::unique_lock<std::mutex> lock(barrier);
                entered = true;
                changed.notify_all();
                changed.wait(lock, [&] { return release; });
                return true;
            });
    });
    { std::unique_lock<std::mutex> lock(barrier);
      assert(changed.wait_for(lock, std::chrono::seconds(2), [&] { return entered; })); }
    assert(owner.onTransportLost(waitingTransport));
    transport.epoch = consent.epoch = 50;
    assert(owner.start(options, transport, consent, provision));
    { std::lock_guard<std::mutex> lock(barrier); release = true; }
    changed.notify_all();
    waiting.join();
    assert(!oldStartResult && owner.running());
    assert(deliver(bootstrap, 1)); // delayed handoff did not revoke the new owner
    owner.stop();
    queue.drain();
    transport.epoch = consent.epoch = 51;
    assert(!owner.start(shortLife, transport, consent, [](const LocalSessionBootstrap&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); return true;
    }));
    assert(!owner.running() && owner.port() == 0);
    queue.drain();
    transport.epoch = consent.epoch = 52;
    { LocalSessionOwner scoped(queue);
      assert(scoped.start(options, transport, consent, provision)); }
    events = queue.drain();
    assert(events.size() == 1 && events.front().epoch == 52);
}
