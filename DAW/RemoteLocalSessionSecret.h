#pragma once
#include <string>

namespace idw::daw::remote {
// 256 bits from the OS CSPRNG, encoded as 64 lowercase hexadecimal characters.
// No weak fallback. On failure the output is unchanged. Never log this value.
bool generateLocalSessionSecret(std::string& output);
}
