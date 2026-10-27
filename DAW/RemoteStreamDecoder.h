#pragma once
#include "RemoteWireFrame.h"
#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>
namespace idw::daw::remote {
class StreamDecoder {
public:
    enum class Result { need_more, message, invalid };
    Result feed(const std::uint8_t* bytes, std::size_t n, std::string& message) {
        if (failed_ || (bytes == nullptr && n != 0)) return Result::invalid;
        if (n > maxBuffered - buffer_.size()) return fail();
        if (n != 0) buffer_.insert(buffer_.end(), bytes, bytes+n);
        if (buffer_.size() < 4) return Result::need_more;
        const std::uint32_t len = (std::uint32_t(buffer_[0])<<24) |
            (std::uint32_t(buffer_[1])<<16) | (std::uint32_t(buffer_[2])<<8) | std::uint32_t(buffer_[3]);
        if (len == 0 || len > maxControlPayload) return fail();
        const auto required = std::size_t(len)+4;
        if (buffer_.size() < required) return Result::need_more;
        std::string candidate(buffer_.begin()+4, buffer_.begin()+required);
        buffer_.erase(buffer_.begin(), buffer_.begin()+required);
        message.swap(candidate);
        return Result::message;
    }
    void reset() { buffer_.clear(); failed_=false; }
    bool failed() const { return failed_; }
private:
    static constexpr std::size_t maxBuffered = (maxControlPayload+4)*2;
    Result fail() { failed_=true; buffer_.clear(); return Result::invalid; }
    std::vector<std::uint8_t> buffer_;
    bool failed_=false;
};
}
