#include "../RemoteAuthenticatedLocalIngress.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <thread>
using namespace idw::daw::remote;

int main() {
    const std::string secret(64, 'a'), replacement(64, 'b');
    auto frame = [&](std::string wire, const std::string& token) {
        std::vector<std::uint8_t> bytes;
        assert(encodeFrame(token + "\n" + wire, bytes));
        return bytes;
    };
    auto command = [&](unsigned sequence, const std::string& token) {
        return frame("1|45|" + std::to_string(sequence) + "|review|arm|1", token);
    };
    TrustedTransportEventQueue queue;
    AuthenticatedLocalIngress ingress(queue);
    assert(!ingress.receiveFrame(command(1, secret)));
    assert(!ingress.beginSession(secret, 45, false, true, true));
    assert(!ingress.beginSession(secret, 45, true, false, true));
    assert(!ingress.beginSession(secret, 45, true, true, false));
    assert(!ingress.beginSession("short", 45, true, true, true));
    assert(!ingress.beginSession(std::string(64, 'z'), 45, true, true, true));
    assert(!ingress.beginSession(secret, 0, true, true, true));
    assert(queue.drain().empty());
    assert(ingress.beginSession(secret, 45, true, true, true));
    assert(!ingress.beginSession(replacement, 46, true, true, true));
    assert(!ingress.receiveFrame(command(99, replacement)));
    assert(!ingress.receiveFrame(frame("1|46|99|wrong|arm|1", secret)));
    assert(!ingress.receiveFrame(frame("1|45|99|record|record|1", secret)));
    assert(!ingress.receiveFrame(frame("1|45|99|track|arm|128", secret)));
    assert(!ingress.receiveFrame(frame("broken", secret)));
    auto incomplete = command(1, secret);
    incomplete.pop_back();
    assert(!ingress.receiveFrame(incomplete));
    auto trailing = command(1, secret);
    trailing.push_back(0);
    assert(!ingress.receiveFrame(trailing));
    assert(!ingress.receiveFrame({255, 255, 255, 255}));
    assert(!ingress.receiveFrame(std::vector<std::uint8_t>(4101, 0)));
    assert(ingress.receiveFrame(command(1, secret)));
    assert(!ingress.receiveFrame(command(1, secret)));
    auto events = queue.drain();
    assert(events.size() == 2);
    assert(events.front().kind == TrustedTransportEvent::Kind::connected);
    assert(events.back().wire == "1|45|1|review|arm|1");
    assert(events.back().wire.find(secret) == std::string::npos);
    // Queue rejection must not consume a sequence: retry after backpressure.
    for (std::size_t i = 0; i < TrustedTransportEventQueue::capacity; ++i)
        assert(queue.push({TrustedTransportEvent::Kind::command, 45,
                           false, false, false, "filler"}));
    assert(!ingress.receiveFrame(command(2, secret)));
    queue.drain();
    assert(ingress.receiveFrame(command(2, secret)));
    // Admission and revocation race: regardless of ordering no command survives.
    std::thread producer([&] { ingress.receiveFrame(command(3, secret)); });
    ingress.endSession();
    producer.join();
    events = queue.drain();
    assert(events.size() == 1);
    assert(events.front().kind == TrustedTransportEvent::Kind::authenticationLost);
    assert(!ingress.receiveFrame(command(4, secret)));
    assert(!ingress.beginSession(secret, 45, true, true, true));
    assert(ingress.beginSession(replacement, 46, true, true, true));
    assert(!ingress.receiveFrame(frame("1|46|1|old|arm|1", secret)));
    assert(ingress.receiveFrame(frame("1|46|1|new|select|0", replacement)));
    ingress.endSession();
    ingress.endSession();
    events = queue.drain();
    assert(events.size() == 1);
    assert(events.front().epoch == 46);
}
