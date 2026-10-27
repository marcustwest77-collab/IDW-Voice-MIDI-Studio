#pragma once
// Glue for decoded frames -> parser -> session policy. No socket, TLS, or DAW dispatch.
#include "RemoteStreamDecoder.h"
#include "RemoteSessionController.h"
#include <cstddef>
#include <cstdint>
#include <string>
namespace idw::daw::remote {
enum class ReceiveResult { needMore, authorized, rejected, failed };
class ReceivePipeline {
public:
    using Parse = bool (*)(const std::string&, CommandMessage&);
    explicit ReceivePipeline(SessionController& session) : session_(session) {}
    ReceiveResult consume(const std::uint8_t* bytes, std::size_t size,
                          const Request& state, Parse parser, CommandMessage& approved) {
        if (failed_ || parser == nullptr) return fail();
        std::string payload;
        const auto status = decoder_.feed(bytes,size,payload);
        if (status == StreamDecoder::Result::invalid) return fail();
        if (status == StreamDecoder::Result::need_more) return ReceiveResult::needMore;
        CommandMessage candidate;
        if (!parser(payload,candidate)) return fail();
        if (session_.inspect(candidate,state) != Decision::accepted)
            return ReceiveResult::rejected;
        approved = candidate;
        return ReceiveResult::authorized;
    }
    // Only reuse on a new authenticated, approved transport/session.
    void reset() { decoder_.reset(); failed_=false; }
    bool failed() const { return failed_; }
private:
    ReceiveResult fail() { failed_=true; return ReceiveResult::failed; }
    SessionController& session_;
    StreamDecoder decoder_;
    bool failed_=false;
};
} // namespace idw::daw::remote
