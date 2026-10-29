#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <Security/SecRandom.h>
#else
#include <unistd.h>
#endif
#include "RemoteLocalSessionSecret.h"
#include <array>

namespace idw::daw::remote {
bool generateLocalSessionSecret(std::string& output) {
    std::array<unsigned char, 32> bytes{};
#if defined(_WIN32)
    if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                       BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return false;
#elif defined(__APPLE__)
    if (SecRandomCopyBytes(kSecRandomDefault, bytes.size(), bytes.data()) != errSecSuccess) return false;
#else
    if (getentropy(bytes.data(), bytes.size()) != 0) return false;
#endif
    static constexpr char hex[] = "0123456789abcdef";
    std::string candidate(64, '0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        candidate[i * 2] = hex[bytes[i] >> 4];
        candidate[i * 2 + 1] = hex[bytes[i] & 15];
    }
    // Best effort wipe of the temporary raw entropy; std::string copies are not
    // guaranteed wiped. The IPC boundary still requires protected provisioning.
    volatile unsigned char* raw = bytes.data();
    for (std::size_t i = 0; i < bytes.size(); ++i) raw[i] = 0;
    output.swap(candidate);
    return true;
}
}
