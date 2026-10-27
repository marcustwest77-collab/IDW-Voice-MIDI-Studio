#pragma once
// Offline integration only. The approved command is data, never executed here.
// Caller MUST bind liveState to actual trusted host DAW state.
#include "RemoteReceivePipeline.h"
#include "RemoteControlCommandParser.h"
namespace idw::daw::remote {
class ValidatedControlReceiver {
public:
    explicit ValidatedControlReceiver(SessionController& session) : pipeline_(session) {}
    ReceiveResult consume(const std::uint8_t* bytes, std::size_t count,
                          const Request& trustedLiveState, CommandMessage& authorized) {
        return pipeline_.consume(bytes,count,trustedLiveState,
                                 &parseControlCommand,authorized);
    }
    void resetForAuthenticatedSession() { pipeline_.reset(); }
    bool failed() const { return pipeline_.failed(); }
private:
    ReceivePipeline pipeline_;
};
} // namespace idw::daw::remote
