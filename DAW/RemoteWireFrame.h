#pragma once
// Bounded length-prefix codec only: never treats decoded payload as authorized.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace idw::daw::remote {
constexpr std::size_t maxControlPayload = 4096;
enum class FrameResult { complete, incomplete, invalid };
inline bool encodeFrame(const std::string& payload, std::vector<std::uint8_t>& out) {
    if (payload.empty() || payload.size() > maxControlPayload) return false;
    const auto n = static_cast<std::uint32_t>(payload.size());
    std::vector<std::uint8_t> bytes;
    bytes.reserve(4 + payload.size());
    bytes.push_back(static_cast<std::uint8_t>((n >> 24) & 255));
    bytes.push_back(static_cast<std::uint8_t>((n >> 16) & 255));
    bytes.push_back(static_cast<std::uint8_t>((n >> 8) & 255));
    bytes.push_back(static_cast<std::uint8_t>(n & 255));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    out.swap(bytes);
    return true;
}
inline FrameResult decodeFrame(const std::vector<std::uint8_t>& data,
                               std::string& out) {
    if (data.size() < 4) return FrameResult::incomplete;
    const std::uint32_t n = (static_cast<std::uint32_t>(data[0]) << 24) |
                            (static_cast<std::uint32_t>(data[1]) << 16) |
                            (static_cast<std::uint32_t>(data[2]) << 8) |
                            static_cast<std::uint32_t>(data[3]);
    if (n == 0 || n > maxControlPayload) return FrameResult::invalid;
    const std::size_t expected = static_cast<std::size_t>(n) + 4;
    if (data.size() < expected) return FrameResult::incomplete;
    if (data.size() != expected) return FrameResult::invalid;
    std::string decoded(data.begin() + 4, data.end());
    out.swap(decoded);
    return FrameResult::complete;
}
} // namespace idw::daw::remote
