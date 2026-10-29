#pragma once
// C++ handoff point for a FUTURE independently authenticated localhost receiver.
// This adapter does not authenticate TLS, JSON, tokens, peer identity or consent.
// Only call after validation by the transport and active host session.
#include "RemoteTrustedTransportEventQueue.h"
#include <cstdint>
#include <string>
#include <utility>

namespace idw::daw::remote {

class VerifiedLocalCommandQueueAdapter {
public:
    explicit VerifiedLocalCommandQueueAdapter(TrustedTransportEventQueue& queue)
        : queue_(queue) {}

    bool submitValidatedCommand(const std::string& wire,
                                std::uint64_t authenticatedEpoch) {
        if (authenticatedEpoch==0 || wire.empty() || wire.size()>4096)
            return false;
        TrustedTransportEvent event;
        event.kind=TrustedTransportEvent::Kind::command;
        event.epoch=authenticatedEpoch;
        event.wire=wire;
        return queue_.push(std::move(event));
    }

private:
    TrustedTransportEventQueue& queue_;
};

} // namespace idw::daw::remote
