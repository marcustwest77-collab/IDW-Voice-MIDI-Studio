#pragma once
#include "RemoteLocalSessionOwner.h"
#include <array>

namespace idw::daw::remote {
constexpr std::size_t localBootstrapSize = 207;
using LocalBootstrapBytes = std::array<unsigned char, localBootstrapSize>;
inline bool validBootstrapHex(const std::string& text) {
    if (text.size() != 64) return false;
    for (char c : text)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
inline bool encodeLocalBootstrap(const LocalSessionBootstrap& value, LocalBootstrapBytes& out) {
    if (value.port == 0 || value.epoch == 0 || !validBootstrapHex(value.secret) ||
        !validBootstrapHex(value.peerSha256) || !validBootstrapHex(value.connectionId)) return false;
    LocalBootstrapBytes bytes{};
    bytes[0] = 'I'; bytes[1] = 'D'; bytes[2] = 'W'; bytes[3] = 'B'; bytes[4] = 1;
    bytes[5] = static_cast<unsigned char>(value.port >> 8);
    bytes[6] = static_cast<unsigned char>(value.port & 255);
    for (unsigned i = 0; i < 8; ++i)
        bytes[7 + i] = static_cast<unsigned char>(value.epoch >> ((7 - i) * 8));
    for (std::size_t i = 0; i < 64; ++i) {
        bytes[15 + i] = static_cast<unsigned char>(value.secret[i]);
        bytes[79 + i] = static_cast<unsigned char>(value.peerSha256[i]);
        bytes[143 + i] = static_cast<unsigned char>(value.connectionId[i]);
    }
    out = bytes;
    return true;
}
inline bool decodeLocalBootstrap(const LocalBootstrapBytes& bytes, LocalSessionBootstrap& out) {
    if (bytes[0] != 'I' || bytes[1] != 'D' || bytes[2] != 'W' || bytes[3] != 'B' || bytes[4] != 1) return false;
    LocalSessionBootstrap value;
    value.port = static_cast<std::uint16_t>((unsigned(bytes[5]) << 8) | bytes[6]);
    for (unsigned i = 0; i < 8; ++i) value.epoch = (value.epoch << 8) | bytes[7 + i];
    value.secret.assign(bytes.begin() + 15, bytes.begin() + 79);
    value.peerSha256.assign(bytes.begin() + 79, bytes.begin() + 143);
    value.connectionId.assign(bytes.begin() + 143, bytes.end());
    LocalBootstrapBytes validated{};
    if (!encodeLocalBootstrap(value, validated)) return false;
    out = std::move(value);
    return true;
}
}
