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
#include "../RemoteLocalSocketReceiver.h"
#include "../RemoteTransportSessionLifecycle.h"
#include "../RemoteValidatedCommandReviewReceiver.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <chrono>
#include <thread>

using namespace idw::daw::remote;
using Clock = std::chrono::steady_clock;
#if defined(_WIN32)
using Socket = SOCKET;
constexpr Socket invalidSocket = INVALID_SOCKET;
void closeSocket(Socket s) { closesocket(s); }
constexpr int writeHalf = SD_SEND;
using AddressLength = int;
#else
using Socket = int;
constexpr Socket invalidSocket = -1;
void closeSocket(Socket s) { close(s); }
constexpr int writeHalf = SHUT_WR;
using AddressLength = socklen_t;
#endif
struct Client {
    Socket s = invalidSocket;
    explicit Client(std::uint16_t port) {
        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        assert(s != invalidSocket);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        assert(connect(s, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    }
    ~Client() { closeSocket(s); }
    void sendBytes(const std::uint8_t* bytes, std::size_t size) {
        std::size_t offset = 0;
        while (offset < size) {
            const auto n = send(s, reinterpret_cast<const char*>(bytes + offset),
                                static_cast<int>(size - offset), 0);
            assert(n > 0);
            offset += static_cast<std::size_t>(n);
        }
    }
    char receipt() {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(s, &reads);
        timeval timeout{};
        timeout.tv_sec = 2;
#if defined(_WIN32)
        assert(select(0, &reads, nullptr, nullptr, &timeout) == 1);
#else
        assert(select(s + 1, &reads, nullptr, nullptr, &timeout) == 1);
#endif
        char byte = 0;
        return recv(s, &byte, 1, 0) == 1 ? byte : 0;
    }
};
std::vector<std::uint8_t> packet(const std::string& token, unsigned epoch,
                                  unsigned seq, std::string action = "arm") {
    std::vector<std::uint8_t> frame;
    assert(encodeFrame(token + "\n1|" + std::to_string(epoch) + "|" +
        std::to_string(seq) + "|request_" + std::to_string(seq) + "|" + action + "|1", frame));
    return frame;
}
char sendFrame(std::uint16_t port, const std::vector<std::uint8_t>& frame, bool fragmented = false) {
    Client client(port);
    if (fragmented) {
        client.sendBytes(frame.data(), 2);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        client.sendBytes(frame.data() + 2, 11);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        client.sendBytes(frame.data() + 13, frame.size() - 13);
    } else client.sendBytes(frame.data(), frame.size());
    assert(shutdown(client.s, writeHalf) == 0);
    return client.receipt();
}
void awaitStopped(LocalSocketReceiver& receiver) {
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (receiver.running() && Clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(!receiver.running());
    assert(receiver.port() == 0);
}

int main() {
    const std::string secret(64, 'a'), wrong(64, 'b');
    TrustedTransportEventQueue queue;
    AuthenticatedLocalIngress ingress(queue);
    LocalSocketReceiver server(ingress);
    LocalSocketReceiver::Options options;
    options.frameTimeoutMs = 200;
    options.sessionTimeoutMs = 15000;
    assert(!server.running() && server.port() == 0);
    assert(!server.start(options, secret, 45, false, true, true));
    assert(queue.drain().empty());
    auto invalidOptions = options;
    invalidOptions.maxConnections = 0;
    assert(!server.start(invalidOptions, secret, 45, true, true, true));
    assert(server.start(options, secret, 45, true, true, true));
    const auto port = server.port();
    assert(port != 0 && server.running());
    assert(!server.start(options, secret, 46, true, true, true));
    // A competing receiver cannot steal the endpoint or create an admission.
    TrustedTransportEventQueue otherQueue;
    AuthenticatedLocalIngress otherIngress(otherQueue);
    LocalSocketReceiver other(otherIngress);
    auto occupied = options;
    occupied.port = port;
    assert(!other.start(occupied, secret, 90, true, true, true));
    assert(otherQueue.drain().empty());
    assert(sendFrame(port, packet(wrong, 45, 99)) == '0');
    assert(sendFrame(port, packet(secret, 46, 99)) == '0');
    assert(sendFrame(port, packet(secret, 45, 99, "record")) == '0');
    auto trailing = packet(secret, 45, 1);
    trailing.push_back(0);
    assert(sendFrame(port, trailing) != '1');
    assert(sendFrame(port, {0, 0, 16, 1}) != '1'); // oversize length, no allocation
    assert(sendFrame(port, {0, 0, 0, 0}) != '1');
    auto truncated = packet(secret, 45, 1);
    truncated.pop_back();
    assert(sendFrame(port, truncated) != '1');
    // An idle connection times out; the next connection can still submit.
    { Client idle(port); assert(idle.receipt() != '1'); }
    // A complete frame without FIN also expires: no command before EOF.
    { Client noFin(port); const auto frame = packet(secret, 45, 1);
      noFin.sendBytes(frame.data(), frame.size()); assert(noFin.receipt() != '1'); }
    // Progress does not reset the total frame deadline (slow-drip protection).
    { Client slow(port); const auto frame = packet(secret, 45, 1);
      slow.sendBytes(frame.data(), 2);
      std::this_thread::sleep_for(std::chrono::milliseconds(120));
      slow.sendBytes(frame.data() + 2, frame.size() - 2);
      std::this_thread::sleep_for(std::chrono::milliseconds(140));
      shutdown(slow.s, writeHalf);
      assert(slow.receipt() != '1'); }
    assert(sendFrame(port, packet(secret, 45, 1), true) == '1');
    assert(sendFrame(port, packet(secret, 45, 1)) == '0'); // new socket, same replay state
    assert(sendFrame(port, packet(secret, 45, 2, "select")) == '1');

    // Exercise the same queue -> lifecycle -> mailbox -> host review path as JUCE.
    SessionReviewCoordinator coordinator;
    RemoteReviewMailbox mailbox;
    TransportSessionLifecycle lifecycle(coordinator, mailbox);
    ValidatedCommandReviewReceiver review(coordinator);
    Request live;
    live.trackCount = 3;
    live.trackIsAudio = true;
    const auto events = queue.drain();
    assert(events.size() == 3);
    for (const auto& event : events) {
        if (event.kind == TrustedTransportEvent::Kind::connected)
            assert(lifecycle.onVerifiedPeerConnected(event.epoch, event.tlsVerified,
                event.hostConsented, event.trackControlGranted));
        else {
            assert(event.kind == TrustedTransportEvent::Kind::command);
            assert(event.wire.find(secret) == std::string::npos);
            assert(lifecycle.onAuthenticatedTrackCommand(event.wire, event.epoch));
        }
    }
    for (const auto& item : mailbox.drain()) assert(review.receive(item.payload, live, item.trustedEpoch));
    assert(coordinator.pending().size() == 2); // Staged only; no DAW action invoked.
    // Queue-full receipt is failure; retry of that sequence succeeds after drain.
    for (std::size_t i = 0; i < TrustedTransportEventQueue::capacity; ++i)
        assert(queue.push({TrustedTransportEvent::Kind::command, 45, false, false, false, "filler"}));
    assert(sendFrame(port, packet(secret, 45, 3)) == '0');
    queue.drain();
    assert(sendFrame(port, packet(secret, 45, 3)) == '1');
    // Stop is responsive even with a connected client withholding its frame.
    { Client idle(port);
      const auto start = Clock::now();
      server.stop();
      assert(Clock::now() - start < std::chrono::seconds(1)); }
    assert(!server.running() && server.port() == 0);
    auto terminal = queue.drain();
    assert(terminal.size() == 1 && terminal.front().kind == TrustedTransportEvent::Kind::authenticationLost);
    lifecycle.onAuthenticationLost();
    assert(coordinator.pending().empty() && !coordinator.grant().connected);
    server.stop();
    assert(queue.drain().empty());
    assert(!server.start(options, secret, 45, true, true, true));
    assert(server.start(options, wrong, 46, true, true, true));
    assert(sendFrame(server.port(), packet(secret, 46, 1)) == '0');
    assert(sendFrame(server.port(), packet(wrong, 46, 1)) == '1');
    server.stop();
    queue.drain();
    // Exhausting the attempt budget fails closed, including rejected clients.
    auto limited = options;
    limited.maxConnections = 1;
    assert(server.start(limited, secret, 47, true, true, true));
    assert(sendFrame(server.port(), packet(wrong, 47, 1)) == '0');
    awaitStopped(server);
    server.stop();
    terminal = queue.drain();
    assert(terminal.size() == 1 && terminal.front().epoch == 47);
    // Session deadline closes idle admissions without requiring another client.
    auto expiring = options;
    expiring.frameTimeoutMs = 20;
    expiring.sessionTimeoutMs = 50;
    assert(server.start(expiring, secret, 48, true, true, true));
    awaitStopped(server);
    server.stop();
    queue.drain();
    // A stale receiver's stop cannot revoke a newer externally owned session.
    assert(ingress.beginSession(wrong, 49, true, true, true));
    ingress.endSession(48); // An old transport cannot revoke a newer admission.
    server.stop();
    assert(ingress.receiveFrame(packet(wrong, 49, 1)));
    ingress.endSession();
    queue.drain();
    // Destruction joins a running worker before the queue/ingress are destroyed.
    { LocalSocketReceiver scoped(ingress);
      assert(scoped.start(options, secret, 50, true, true, true)); }
    terminal = queue.drain();
    assert(terminal.size() == 1 && terminal.front().epoch == 50);
}
