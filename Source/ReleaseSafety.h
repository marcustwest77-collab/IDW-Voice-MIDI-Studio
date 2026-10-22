#pragma once

#include <algorithm>
#include <cmath>

namespace idw {

struct QualityPlan {
    bool formantEnabled = true;
    int maximumHarmonyVoices = 2;
    int formantAnalysisHop = 256;
};

inline QualityPlan qualityPlan(int mode) {
    if (mode <= 0) return {false, 1, 512};       // Eco
    if (mode >= 2) return {true, 2, 128};        // High
    return {true, 2, 256};                       // Studio
}

inline float guardedOutput(float sample, bool enabled) {
    if (!std::isfinite(sample)) return 0.0f;
    if (!enabled) return sample;
    constexpr float knee = 0.94f;
    constexpr float ceiling = 0.999f;
    const float magnitude = std::abs(sample);
    if (magnitude <= knee) return sample;
    const float compressed = knee + (ceiling - knee) * (1.0f - std::exp(-(magnitude - knee) / (ceiling - knee)));
    return std::copysign(std::min(ceiling, compressed), sample);
}

class ConfidenceGate {
public:
    void prepare(double sampleRate) {
        const auto rate = std::max(8000.0, sampleRate);
        attack = static_cast<float>(1.0 - std::exp(-1.0 / (rate * 0.018)));
        release = static_cast<float>(1.0 - std::exp(-1.0 / (rate * 0.090)));
        envelope = 0.0f;
    }

    float process(bool open) {
        const float target = open ? 1.0f : 0.0f;
        envelope += (open ? attack : release) * (target - envelope);
        if (envelope < 1.0e-5f) envelope = 0.0f;
        return envelope;
    }

    float value() const { return envelope; }

private:
    float attack = 0.001f, release = 0.0002f, envelope = 0.0f;
};

} // namespace idw
